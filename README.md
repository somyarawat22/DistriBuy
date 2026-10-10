# OS + DBMS Project: Distributed, Fault-Tolerant PostgreSQL E-Commerce System

A distributed, fault-tolerant e-commerce system using PostgreSQL as the storage engine for each node. The partitioning, Coordinator, replication orchestration, distributed transactions, failure detection, and recovery logic are built by this project.

## Current Status

The project is functionally complete against its MVP: a working distributed, fault-tolerant e-commerce backend with a customer storefront and an admin dashboard, verified live against a real 3-node PostgreSQL cluster.

**Distributed database layer**

- **3 independent PostgreSQL clusters** (node1/5433 Users, node2/5434 Products, node3/5435 Orders), each owning one business domain's table.
- **Coordinator** (`core/src/coordinator.cpp`, C++ / `libpqxx`): routes requests by domain, reports quorum-based cluster health, runs **real Two-Phase Commit** (`PREPARE TRANSACTION` / `COMMIT PREPARED` / `ROLLBACK PREPARED`) for orders across the Orders and Products nodes (commit and abort paths both verified), **fails reads over to the replica node** when a primary is unreachable, and does **continuous heartbeat monitoring** (`--watch`) that detects both failure and recovery without a restart.
- **PostgreSQL logical replication ring** (Users to node2, Products to node3, Orders to node1), set up by `scripts/setup_replication.sh`.
- **Per-node process** (`core/src/main.cpp`): connects to its own node's database via `libpqxx`.

**Application layer**

- **REST API** (`api/server.js`, Node.js/Express): the same routing, failover and 2PC logic over HTTP, plus real authentication (consumer sign-up/login with bcrypt-hashed passwords, a separate admin login, token sessions) and server-side authorization. Consumers can only read their own orders; admin-only routes are enforced on the server, not just hidden in the UI.
- **Storefront** (`frontend/`): login page (Consumer / Admin), product search, cart, checkout with a delivery address (Cash on Delivery), and a "My Orders" view with live status (placed / shipped / delivered / cancelled).
- **Admin dashboard**: Overview (stats, including an order-value total computed by joining Orders and Products across two nodes, plus live cluster health), and Products / Orders / Users management (add, edit and delete products; update order status; delete users).

Not built, by design: see **Known limitations** and the stretch goals below.

## The core problem you're solving

A normal e-commerce app looks like this:

```text
Users → One Server → One Database(cannot handle when the users becomes large)
```

This works for a college demo with 10 users, but it has one fatal flaw: if that database crashes, the entire website goes down. This is a single point of failure.

The answer is to split data across multiple independent PostgreSQL-backed nodes and make copies of important data so if one node dies, another can immediately take over.

## The two layers of the system

### 1. Application Layer

This is what users see and use:

- browse products, search, and filter
- add to cart and checkout
- login and register
- view order history
- admin dashboard to manage products, orders, and system health

### 2. Distributed Database Layer

This is the actual engineering core of the project:

- multiple PostgreSQL-backed nodes running independently
- data split across nodes
- copies of data for safety
- a coordinator routing every request to the correct place
- transaction handling so operations do not leave data half-broken
- automatic detection of dead nodes and recovery when they return

## Core concepts

### Nodes

A node is an independent process with its own PostgreSQL database that owns a slice of data. In the finalized design, each node owns a full business domain, not a numeric key range:

| Node | Owns |
|---|---|
| Node 1 | Users |
| Node 2 | Products & Inventory |
| Node 3 | Orders & Payments |

The prototype models these nodes with localhost endpoints on ports 5433, 5434, and 5435, matching the PostgreSQL clusters for node 1, node 2, and node 3 running locally.

### Partitioning

Instead of one giant table spanning one machine, each table lives on one node. This spreads storage and request load, and a problemin one domain does not bring down the entire system.

### Replication

Partitioning alone is risky. If one node fails, data becomes unreachable. PostgreSQL logical replication copies each node's data toanother node in a ring:

```text
Node 1 (Users) → replicated to → Node 2
Node 2 (Products) → replicated to → Node 3
Node 3 (Orders) → replicated to → Node 1
```

If Node 2 fails, Node 3 already has a copy of Products data and can take over.

### Query Coordinator

The frontend and application layer never talk directly to nodes. They talk only to the Coordinator.

The coordinator:

- knows which node owns which data
- routes requests to the right node
- runs distributed transactions across multiple nodes
- sends heartbeat checks
- triggers failover when a node dies

This hides the complexity from the application layer, and is now implemented in `core/src/coordinator.cpp` (routing, failover, 2PC, heartbeat monitoring) and mirrored in `api/server.js` for HTTP access.

## Concurrency control

A key issue is the last-item-in-stock problem.

Without protection:

```text
Both read stock = 1
Both think it is available
Both buy
Stock becomes -1
```

The solution is Two-Phase Locking (2PL): whoever touches the row first acquires a lock; others wait until it is released.

## Transactions

Placing an order is not one operation. It is multiple steps, such as:

- create order
- reduce stock
- process payment

These must succeed together or fail together. The implementation uses BEGIN → ... → COMMIT and ROLLBACK when necessary.

## Distributed transactions and 2PC

Because an order may span multiple nodes, a single purchase can involve multiple nodes. Two-Phase Commit (2PC) ensures both nodes agree before finalization:

### Phase 1: Prepare

The coordinator asks each node if it is ready to commit.

### Phase 2: Commit or Abort

If all say yes, the coordinator tells them to commit. If any says no, everyone rolls back.

This prevents half-finished updates.

## Failure detection, failover, and recovery

- detection: the coordinator pings nodes using heartbeats
- failover: traffic is redirected to the replica of a failed node
- recovery: PostgreSQL replays its WAL when a node restarts and logical replication catches it up on its own; the Coordinator's heartbeat notices the node answering again and resumes routing to it (it does not itself measure replication lag)

This complete lifecycle has been demonstrated live: killing node2's PostgreSQL cluster mid-session causes the Coordinator's heartbeat loop to detect it within seconds, reads automatically fail over to node3 (which holds the replicated Products data), writes correctly refuse to go through the dead primary, and restarting node2 is automatically detected as a recovery.

## OS concepts reflected in the project

| OS topic | Where it appears |
|---|---|
| Processes | each node, coordinator, and API server is a separate process |
| Threads / concurrency | concurrent requests are handled by the API's async event loop and by PostgreSQL's per-connection backend processes; the C++ Coordinator itself is single-threaded |
| Synchronization | PostgreSQL row-level locks (`SELECT ... FOR UPDATE`) prevent race conditions such as overselling the last item |
| IPC | nodes and coordinator communicate via TCP sockets |
| Deadlock | could arise when transactions wait for locks |
| File management | each PostgreSQL node persists tables and WAL in its own data directory |

## Tech stack

- C++ (Coordinator, per-node process), using `libpqxx` for PostgreSQL connections
- TCP sockets (both the Postgres wire protocol used by `libpqxx`, and PostgreSQL's own logical replication traffic between nodes)
- PostgreSQL (per-node storage, 2PC, row-level locking, logical replication)
- Node.js/Express + the `pg` driver (REST API)
- React, loaded via CDN as a single static HTML file, no build tooling (frontend)

## Why this project is valuable

This project touches the same skills backend and infrastructure interviews test:

- distributed systems
- concurrency
- networking
- transaction management
- fault tolerance
- failover design


Contributions are welcome as the project grows. For now, the work is focused on learning by building the system incrementally and validating each step with small code changes.
