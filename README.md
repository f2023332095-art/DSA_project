Overview
A single-file C++ interactive parking simulator that demonstrates core data structures and simple allocation logic.
It models zones, slots, and requests, supports entry, occupy, exit, cancel, search, rollback, and shows a live dashboard with revenue and utilization.

Key ideas

Time is simulated with integer ticks.

Allocation prefers same-zone slots; cross-zone allocations incur a penalty.

Charges = duration × rate_per_tick + penalty.

Uses stack, queue, linked list, BST, and heapify to illustrate common algorithms and patterns.

Features
Interactive console menu to manage zones, slots, and vehicles.

Automatic allocation with priority heap (same-zone preferred).

Pending queue for requests when full (FIFO).

Rollback stack to undo recent allocations (LIFO).

Simple BST index for fast slot lookup by id.

Billing and revenue tracking.

Search by vehicle id and exit by vehicle id.

Single-file implementation for easy compilation and sharing.
