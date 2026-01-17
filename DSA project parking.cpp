// smart_parking_interactive_with_using_std.cpp
// Single-file parking system (stack, queue, list, BST, heapify)
// Interactive dashboard: add zones/slots, entry, exit, search, cancel, rollback, stats
// Compile: g++ -std=c++17 smart_parking_interactive_with_using_std.cpp -O2 -o smart_parking

#include <iostream>
#include <vector>
#include <list>
#include <queue>
#include <stack>
#include <algorithm>
#include <string>
#include <memory>
#include <limits>

using namespace std;

// -------------------- Enums --------------------
enum class SlotStatus { FREE, ALLOCATED, OCCUPIED };
enum class RequestState { REQUESTED, ALLOCATED, OCCUPIED, RELEASED, CANCELLED };

// -------------------- ParkingSlot --------------------
class ParkingSlot {
public:
    int id;
    int zoneId;
    SlotStatus status;
    int occupiedStartTick;

    ParkingSlot(int id_ = -1, int zoneId_ = -1)
        : id(id_), zoneId(zoneId_), status(SlotStatus::FREE), occupiedStartTick(-1) {}

    bool isFree() const { return status == SlotStatus::FREE; }
    void allocate() { status = SlotStatus::ALLOCATED; }
    void occupy(int tick) { status = SlotStatus::OCCUPIED; occupiedStartTick = tick; }
    void release() { status = SlotStatus::FREE; occupiedStartTick = -1; }
};

// -------------------- ParkingRequest --------------------
class ParkingRequest {
    int id;
    string vehicleId;
    int requestedZone;
    RequestState state;
    int allocatedSlotId;
    int allocatedZoneId;
    int requestTick;
    int startTick;
    int endTick;
    double penalty;

public:
    ParkingRequest(int id_, const string &vid, int zone, int tick)
        : id(id_), vehicleId(vid), requestedZone(zone), state(RequestState::REQUESTED),
          allocatedSlotId(-1), allocatedZoneId(-1), requestTick(tick),
          startTick(-1), endTick(-1), penalty(0.0) {}

    int getId() const { return id; }
    const string& getVehicleId() const { return vehicleId; }
    int getRequestedZone() const { return requestedZone; }
    RequestState getState() const { return state; }
    int getAllocatedSlotId() const { return allocatedSlotId; }
    int getAllocatedZoneId() const { return allocatedZoneId; }
    double getPenalty() const { return penalty; }
    void setPenalty(double p) { penalty = p; }

    int getDurationTicks() const { return (startTick >= 0 && endTick >= 0) ? (endTick - startTick) : 0; }

    bool transition(RequestState newState, int currentTick) {
        if (state == RequestState::REQUESTED) {
            if (newState == RequestState::ALLOCATED || newState == RequestState::CANCELLED) {
                state = newState;
                if (newState == RequestState::ALLOCATED) startTick = currentTick;
                return true;
            }
            return false;
        }
        if (state == RequestState::ALLOCATED) {
            if (newState == RequestState::OCCUPIED || newState == RequestState::CANCELLED) {
                state = newState;
                if (newState == RequestState::OCCUPIED) startTick = currentTick;
                return true;
            }
            return false;
        }
        if (state == RequestState::OCCUPIED) {
            if (newState == RequestState::RELEASED) {
                state = newState;
                endTick = currentTick;
                return true;
            }
            return false;
        }
        return false;
    }

    void setAllocation(int slotId, int zoneId) { allocatedSlotId = slotId; allocatedZoneId = zoneId; }
    void clearAllocation() { allocatedSlotId = -1; allocatedZoneId = -1; }
};

// -------------------- Zone --------------------
class Zone {
public:
    int id;
    vector<ParkingSlot> slots;
    Zone(int id_ = -1) : id(id_) {}
    void addSlots(int count, int startIdBase) {
        for (int i = 0; i < count; ++i) slots.emplace_back(startIdBase + i, id);
    }
    int freeCount() const {
        int cnt = 0;
        for (const auto &s : slots) if (s.isFree()) ++cnt;
        return cnt;
    }
};

// -------------------- Simple BST for slot lookup --------------------
class SlotBST {
    struct Node {
        int key;
        ParkingSlot* slotPtr;
        Node* left;
        Node* right;
        Node(int k = -1, ParkingSlot* p = nullptr) : key(k), slotPtr(p), left(nullptr), right(nullptr) {}
    };
    Node* root;

    void insertNode(Node*& cur, int key, ParkingSlot* p) {
        if (!cur) { cur = new Node(key, p); return; }
        if (key < cur->key) insertNode(cur->left, key, p);
        else if (key > cur->key) insertNode(cur->right, key, p);
        else cur->slotPtr = p;
    }

    ParkingSlot* findNode(Node* cur, int key) const {
        if (!cur) return nullptr;
        if (key == cur->key) return cur->slotPtr;
        if (key < cur->key) return findNode(cur->left, key);
        return findNode(cur->right, key);
    }

    Node* removeNode(Node* cur, int key) {
        if (!cur) return nullptr;
        if (key < cur->key) cur->left = removeNode(cur->left, key);
        else if (key > cur->key) cur->right = removeNode(cur->right, key);
        else {
            if (!cur->left) { Node* r = cur->right; delete cur; return r; }
            if (!cur->right) { Node* l = cur->left; delete cur; return l; }
            Node* succ = cur->right;
            while (succ->left) succ = succ->left;
            cur->key = succ->key;
            cur->slotPtr = succ->slotPtr;
            cur->right = removeNode(cur->right, succ->key);
        }
        return cur;
    }

    void destroy(Node* cur) {
        if (!cur) return;
        destroy(cur->left);
        destroy(cur->right);
        delete cur;
    }

public:
    SlotBST() : root(nullptr) {}
    ~SlotBST() { destroy(root); }
    void insert(int key, ParkingSlot* p) { insertNode(root, key, p); }
    ParkingSlot* find(int key) const { return findNode(root, key); }
    void remove(int key) { root = removeNode(root, key); }
};

// -------------------- AllocationEngine with heapify --------------------
struct AllocationResult { int slotId; int zoneId; double penalty; AllocationResult(int s=-1,int z=-1,double p=0.0):slotId(s),zoneId(z),penalty(p){} };
class AllocationEngine {
public:
    static constexpr double CROSS_ZONE_PENALTY = 5.0;
    static AllocationResult allocate(int requestedZone, vector<Zone> &zones) {
        struct Candidate { int slotId; int zoneId; double penalty; };
        vector<Candidate> candidates;
        for (int z = 0; z < (int)zones.size(); ++z) {
            for (auto &slot : zones[z].slots) {
                if (slot.isFree()) {
                    double p = (z == requestedZone) ? 0.0 : CROSS_ZONE_PENALTY;
                    candidates.push_back({slot.id, z, p});
                }
            }
        }
        if (candidates.empty()) return AllocationResult(-1,-1,0.0);
        auto cmp = [](const Candidate &a, const Candidate &b) {
            if (a.penalty != b.penalty) return a.penalty > b.penalty;
            return a.slotId > b.slotId;
        };
        make_heap(candidates.begin(), candidates.end(), cmp);
        Candidate best = candidates.front();
        for (auto &s : zones[best.zoneId].slots) {
            if (s.id == best.slotId) { s.allocate(); break; }
        }
        return AllocationResult(best.slotId, best.zoneId, best.penalty);
    }
};

// -------------------- RollbackAction & RollbackManager (stack) --------------------
struct RollbackAction { int requestId; int slotId; int zoneId; RequestState prevState; RollbackAction(int r=-1,int s=-1,int z=-1,RequestState p=RequestState::REQUESTED):requestId(r),slotId(s),zoneId(z),prevState(p){} };
class RollbackManager {
    stack<RollbackAction> st;
public:
    void push(const RollbackAction &a) { st.push(a); }
    bool canRollback() const { return !st.empty(); }
    RollbackAction pop() { RollbackAction a = st.top(); st.pop(); return a; }
    int size() const { return (int)st.size(); }
};

// -------------------- ParkingSystem --------------------
class ParkingSystem {
    vector<Zone> zones;
    list<ParkingRequest*> history; // linked list
    RollbackManager rollbackManager;    // stack
    queue<int> pendingRequests;    // queue
    SlotBST slotIndex;                  // BST
    int nextRequestId;
    int tickCounter;
    double totalRevenue;
    double ratePerTick;

public:
    ParkingSystem(int zoneCount = 0, double rate = 1.0)
        : nextRequestId(1), tickCounter(0), totalRevenue(0.0), ratePerTick(rate) {
        for (int i = 0; i < zoneCount; ++i) zones.emplace_back(i);
    }

    ~ParkingSystem() {
        for (auto p : history) delete p;
    }

    void addZone() {
        int id = (int)zones.size();
        zones.emplace_back(id);
        cout << "Added zone " << id << "\n";
    }

    void addSlotsToZoneInteractive(int zoneId, int count) {
        if (zoneId < 0 || zoneId >= (int)zones.size()) { cout << "Invalid zone\n"; return; }
        int base = zoneId * 1000 + zones[zoneId].slots.size();
        zones[zoneId].addSlots(count, base);
        for (int i = 0; i < count; ++i) {
            ParkingSlot* ptr = &zones[zoneId].slots[zones[zoneId].slots.size() - count + i];
            slotIndex.insert(ptr->id, ptr);
        }
        cout << "Added " << count << " slots to zone " << zoneId << "\n";
    }

    void tick() { ++tickCounter; }

    // entry: create request and try allocate immediately; returns request id
    int entry(const string &vehicleId, int requestedZone, double &outPenalty) {
        tick();
        ParkingRequest* req = new ParkingRequest(nextRequestId++, vehicleId, requestedZone, tickCounter);
        history.push_back(req);
        AllocationResult res = AllocationEngine::allocate(requestedZone, zones);
        if (res.slotId == -1) {
            pendingRequests.push(req->getId());
            outPenalty = 0.0;
            cout << "No slot available now. Request queued (id=" << req->getId() << ")\n";
            return req->getId();
        }
        req->setAllocation(res.slotId, res.zoneId);
        req->setPenalty(res.penalty);
        req->transition(RequestState::ALLOCATED, tickCounter);
        rollbackManager.push(RollbackAction(req->getId(), res.slotId, res.zoneId, RequestState::REQUESTED));
        outPenalty = res.penalty;
        cout << "Allocated slot " << res.slotId << " in zone " << res.zoneId << " (penalty " << res.penalty << ")\n";
        return req->getId();
    }

    // exit by vehicle id: find request and release
    bool exitByVehicle(const string &vehicleId) {
        for (auto *r : history) {
            if (r->getVehicleId() == vehicleId) {
                if (r->getState() == RequestState::OCCUPIED || r->getState() == RequestState::ALLOCATED) {
                    return release(r->getId());
                }
            }
        }
        return false;
    }

    bool occupy(int requestId) {
        tick();
        ParkingRequest* r = findRequest(requestId);
        if (!r) { cout << "Request not found\n"; return false; }
        if (!r->transition(RequestState::OCCUPIED, tickCounter)) { cout << "Invalid transition\n"; return false; }
        ParkingSlot* s = slotIndex.find(r->getAllocatedSlotId());
        if (s) s->occupy(tickCounter);
        cout << "Request " << requestId << " is now OCCUPIED\n";
        return true;
    }

    bool release(int requestId) {
        tick();
        ParkingRequest* r = findRequest(requestId);
        if (!r) { cout << "Request not found\n"; return false; }
        if (!r->transition(RequestState::RELEASED, tickCounter)) { cout << "Invalid transition\n"; return false; }
        int duration = r->getDurationTicks();
        double charge = duration * ratePerTick + r->getPenalty();
        totalRevenue += charge;
        ParkingSlot* s = slotIndex.find(r->getAllocatedSlotId());
        if (s) {
            s->release();
            tryAllocatePending();
        }
        cout << "Released request " << requestId << ". Duration: " << duration << " ticks. Charge: " << charge << "\n";
        return true;
    }

    bool cancel(int requestId) {
        tick();
        ParkingRequest* r = findRequest(requestId);
        if (!r) { cout << "Request not found\n"; return false; }
        if (!r->transition(RequestState::CANCELLED, tickCounter)) { cout << "Invalid transition\n"; return false; }
        if (r->getAllocatedSlotId() != -1) {
            ParkingSlot* s = slotIndex.find(r->getAllocatedSlotId());
            if (s) {
                s->release();
                tryAllocatePending();
            }
            r->clearAllocation();
        }
        cout << "Cancelled request " << requestId << "\n";
        return true;
    }

    bool rollbackLastK(int k) {
        if (k <= 0) { cout << "k must be > 0\n"; return false; }
        if (rollbackManager.size() < k) { cout << "Not enough actions to rollback\n"; return false; }
        for (int i = 0; i < k; ++i) {
            RollbackAction a = rollbackManager.pop();
            ParkingRequest* r = findRequest(a.requestId);
            if (!r) continue;
            if (a.slotId != -1) {
                ParkingSlot* s = slotIndex.find(a.slotId);
                if (s) s->release();
            }
            int id = a.requestId;
            string vid = r->getVehicleId();
            int reqZone = r->getRequestedZone();
            for (auto it = history.begin(); it != history.end(); ++it) {
                if ((*it)->getId() == id) {
                    delete *it;
                    *it = new ParkingRequest(id, vid, reqZone, tickCounter);
                    break;
                }
            }
        }
        cout << "Rolled back " << k << " allocations\n";
        return true;
    }

    // search car by vehicle id; returns request id or -1
    int searchCar(const string &vehicleId) const {
        for (const auto *r : history) if (r->getVehicleId() == vehicleId) return r->getId();
        return -1;
    }

    // dashboard and analytics
    void showDashboard() const {
        cout << "\n-------------------- DASHBOARD --------------------\n";
        cout << "Tick: " << tickCounter << "\n";
        cout << "Total Revenue: " << totalRevenue << "\n";
        cout << "Rate per tick: " << ratePerTick << "\n\n";
        cout << "Zone empty slots and IDs:\n";
        for (const auto &z : zones) {
            cout << " Zone " << z.id << " - Free: " << z.freeCount() << "  [";
            bool first = true;
            for (const auto &s : z.slots) {
                if (s.isFree()) {
                    if (!first) cout << ", ";
                    cout << s.id;
                    first = false;
                }
            }
            cout << "]\n";
        }
        cout << "\nRequests (id vehicle state slot):\n";
        for (const auto *r : history) {
            cout << " " << r->getId() << " " << r->getVehicleId()
                      << " " << static_cast<int>(r->getState())
                      << " " << r->getAllocatedSlotId() << "\n";
        }
        cout << "Pending queue size: " << pendingRequests.size() << "\n";
        cout << "---------------------------------------------------\n\n";
    }

    double averageParkingDurationTicks() const {
        double total = 0.0; int count = 0;
        for (const auto *r : history) {
            if (r->getDurationTicks() > 0) { total += r->getDurationTicks(); ++count; }
        }
        return count ? total / count : 0.0;
    }

    vector<int> zoneUtilization() const {
        vector<int> res;
        for (const auto &z : zones) res.push_back(z.freeCount());
        return res;
    }

    int completedCount() const {
        int c = 0;
        for (const auto *r : history) if (r->getDurationTicks() > 0) ++c;
        return c;
    }

    int cancelledCount() const {
        int c = 0;
        for (const auto *r : history) if (r->getState() == RequestState::CANCELLED) ++c;
        return c;
    }

    ParkingRequest* findRequest(int requestId) {
        for (auto *r : history) if (r->getId() == requestId) return r;
        return nullptr;
    }

    double getTotalRevenue() const { return totalRevenue; }

private:
    // Try to allocate pending requests (FIFO)
    void tryAllocatePending() {
        int attempts = (int)pendingRequests.size();
        while (attempts-- > 0 && !pendingRequests.empty()) {
            int rid = pendingRequests.front();
            pendingRequests.pop();
            ParkingRequest* r = findRequest(rid);
            if (!r) continue;
            if (r->getState() != RequestState::REQUESTED) continue;
            AllocationResult res = AllocationEngine::allocate(r->getRequestedZone(), zones);
            if (res.slotId == -1) {
                pendingRequests.push(rid);
                break;
            }
            r->setAllocation(res.slotId, res.zoneId);
            r->setPenalty(res.penalty);
            r->transition(RequestState::ALLOCATED, tickCounter);
            rollbackManager.push(RollbackAction(r->getId(), res.slotId, res.zoneId, RequestState::REQUESTED));
            cout << "Pending request " << rid << " allocated slot " << res.slotId << "\n";
        }
    }
};

// -------------------- Utility: read int safely --------------------
int readInt() {
    int x;
    while (!(cin >> x)) {
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        cout << "Please enter a valid number: ";
    }
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
    return x;
}

// -------------------- Interactive menu --------------------
int main() {
    ParkingSystem ps;
    cout << "Welcome to Smart Parking Interactive Dashboard\n";
    cout << "Start by adding zones and slots.\n";

    bool running = true;
    while (running) {
        cout << "\nMenu:\n"
                  << " 1) Add zone\n"
                  << " 2) Add slots to zone\n"
                  << " 3) Vehicle entry (create request)\n"
                  << " 4) Vehicle occupy (by request id)\n"
                  << " 5) Vehicle exit (by vehicle id)\n"
                  << " 6) Cancel request (by id)\n"
                  << " 7) Search vehicle\n"
                  << " 8) Rollback last K allocations\n"
                  << " 9) Show dashboard\n"
                  << "10) Show stats\n"
                  << "0) Quit\n"
                  << "Choose option: ";
        int opt = readInt();
        switch (opt) {
            case 1: {
                ps.addZone();
                break;
            }
            case 2: {
                cout << "Zone id: ";
                int zid = readInt();
                cout << "Number of slots to add: ";
                int cnt = readInt();
                ps.addSlotsToZoneInteractive(zid, cnt);
                break;
            }
            case 3: {
                cout << "Vehicle ID (string): ";
                string vid; getline(cin, vid);
                if (vid.empty()) { getline(cin, vid); }
                cout << "Requested zone id: ";
                int zid = readInt();
                double pen = 0.0;
                int rid = ps.entry(vid, zid, pen);
                cout << "Request created id=" << rid << " penalty=" << pen << "\n";
                break;
            }
            case 4: {
                cout << "Request id to occupy: ";
                int rid = readInt();
                ps.occupy(rid);
                break;
            }
            case 5: {
                cout << "Vehicle ID to exit: ";
                string vid; getline(cin, vid);
                if (vid.empty()) { getline(cin, vid); }
                bool ok = ps.exitByVehicle(vid);
                cout << (ok ? "Exit processed\n" : "Vehicle not found or not occupying\n");
                break;
            }
            case 6: {
                cout << "Request id to cancel: ";
                int rid = readInt();
                ps.cancel(rid);
                break;
            }
            case 7: {
                cout << "Vehicle ID to search: ";
                string vid; getline(cin, vid);
                if (vid.empty()) { getline(cin, vid); }
                int found = ps.searchCar(vid);
                if (found == -1) cout << "Not found\n"; else cout << "Found request id: " << found << "\n";
                break;
            }
            case 8: {
                cout << "K to rollback: ";
                int k = readInt();
                ps.rollbackLastK(k);
                break;
            }
            case 9: {
                ps.showDashboard();
                break;
            }
            case 10: {
                cout << "Completed: " << ps.completedCount()
                          << " Cancelled: " << ps.cancelledCount()
                          << " AvgTicks: " << ps.averageParkingDurationTicks()
                          << " Revenue: " << ps.getTotalRevenue() << "\n";
                break;
            }
            case 0: running = false; break;
            default: cout << "Invalid option\n"; break;
        }
    }

    cout << "Goodbye\n";
    return 0;
}
