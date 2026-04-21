# Phase 2 Workload Plan

## Goal

Implement the Phase 2 B+Tree design on top of the current storage engine with a 5-person team, while minimizing merge conflicts and keeping dependencies clear.

## Current-Code Impact Summary

Phase 2 requires changing existing code, not just adding new files.

Most impacted areas:

1. `derived_page/include/node_page.h`
2. `derived_page/src/node_page.cc`
3. `btree/src/btree_bt_cursor_private.cc`
4. `btree/src/btree_bt_cursor_public.cc`
5. `btree/src/btree_balance.cc`
6. `btree/src/btree.cc`
7. `btree/test/*`

Key design gap versus current code:

1. Current tree logic is B-tree-oriented, not B+Tree-oriented.
2. Internal nodes currently participate in data navigation semantics.
3. There is no `is_leaf` field in the node-page header.
4. `right_child` is not yet used as `next_leaf` for leaf pages.
5. `BtreeCreateIndex()` currently behaves the same as `BtreeCreateTable()`.
6. Hidden `ROWID` and true secondary index behavior are not implemented.

## Team Split

Suggested ownership by teammate:

1. **Siwei Tan**: node/page format changes
2. **Mengchao Ren**: traversal and cursor behavior
3. **Bruce Cheng**: split, rebalance, merge logic
4. **Dejian Huang**: table/index creation, metadata, hidden `ROWID`
5. **Xintong Bi**: secondary index workflow and testing/integration

## Workstream 1: Node Format and Page Helpers

**Owner:** Siwei Tan

**Primary files:**

1. `derived_page/include/node_page.h`
2. `derived_page/src/node_page.cc`

**Deliverables:**

1. Add `is_leaf` to `NodePageHeaderByteView`.
2. Add helper methods for leaf/internal interpretation.
3. Reuse `right_child` as `next_leaf` on leaf pages.
4. Update `ZeroPage()` so a page can be initialized as an empty leaf or internal node.
5. Preserve the single physical cell format:
   - internal routing cell: `data_size == 0`
   - leaf data cell: `data_size > 0`

**Acceptance criteria:**

1. Empty page can be created as a leaf.
2. Empty page can be created as an internal node.
3. Existing cell operations still work for both leaf and internal pages.
4. Leaf-link getter/setter behavior is correct.

## Workstream 2: Traversal and Cursor Semantics

**Owner:** Mengchao Ren

**Primary files:**

1. `btree/src/btree_bt_cursor_private.cc`
2. `btree/src/btree_bt_cursor_public.cc`

**Deliverables:**

1. Update `MoveToLeftmost()` to end on the leftmost leaf.
2. Update `BtreeFirst()` so it always positions the cursor on a leaf.
3. Update `BtreeMoveTo()` so internal nodes are routing-only and search ends at a leaf.
4. Update `BtreeNext()` to use `next_leaf` instead of climbing through parents for sequential scans.
5. Preserve current cursor API shape so upper layers do not need large interface changes.

**Acceptance criteria:**

1. Full scan starts at the first record in the leftmost leaf.
2. Search for a key ends on the correct leaf.
3. `BtreeNext()` can move across leaf boundaries.
4. Cursor behavior remains valid after splits.

## Workstream 3: Split, Rebalance, and Merge

**Owner:** Bruce Cheng

**Primary files:**

1. `btree/src/btree_balance.cc`
2. `btree/src/btree_bt_cursor_public.cc` for delete-side behavior

**Deliverables:**

1. Convert balancing logic from B-tree semantics to B+Tree semantics.
2. Implement leaf split behavior:
   - split data across two leaves
   - copy separator key to parent
   - keep separator key in leaf
   - update `next_leaf`
3. Implement internal split behavior:
   - push separator up to parent
   - remove separator from the child level
4. Implement leaf redistribution/merge rules.
5. Implement internal redistribution/merge rules.
6. Ensure parent separator updates are correct after rebalance.
7. Ensure `ReParentChildPages(...)` remains correct after structure changes.

**Acceptance criteria:**

1. Overflowing leaf splits correctly.
2. Overflowing internal node splits correctly.
3. Deletion rebalance preserves tree invariants.
4. Leaf chain remains valid after split/merge.
5. Root split and root collapse cases work.

## Workstream 4: Table/Index Creation, Metadata, Hidden ROWID

**Owner:** Dejian Huang

**Primary files:**

1. `btree/src/btree.cc`
2. Any metadata-related usage through `BtreeGetMeta` / `BtreeUpdateMeta`

**Deliverables:**

1. Update `BtreeCreateTable()` to initialize a leaf root page explicitly.
2. Make `BtreeCreateIndex()` a distinct path, not just `BtreeCreateTable()`.
3. Define how index root page numbers are stored in metadata for this phase.
4. Define a minimal hidden `ROWID` strategy for tables without PKs.
5. Define a consistent serialized format for `ROWID` keys.
6. Document how table root pages and index root pages are registered for testing/demo purposes.

**Acceptance criteria:**

1. New table root is a valid empty leaf.
2. New index root is a valid empty leaf.
3. Metadata can retrieve created root page numbers.
4. Hidden `ROWID` generation is deterministic and unique.

## Workstream 5: Secondary Indexes and Test Integration

**Owner:** Xintong Bi

**Primary files:**

1. `btree/test/*`
2. Any index-build/search helper code added for Phase 2

**Deliverables:**

1. Define secondary-index entry format.
2. Recommended format:
   - index key: composite `(secondary_key, PK_or_ROWID)`
   - leaf payload: `PK_or_ROWID`
3. Implement index build flow by scanning the main table.
4. Implement lookup flow for secondary index.
5. Add tests for duplicate secondary keys.
6. Add tests for two-step lookup from secondary index to main table.
7. Act as integration owner for cross-branch validation.

**Acceptance criteria:**

1. Duplicate secondary-key values are supported.
2. Composite index keys are unique.
3. Secondary lookup returns the correct row reference.
4. Tests cover create/search/scan/basic delete interactions.

## Dependency Order

Recommended implementation order:

1. Workstream 1: node format and helpers
2. Workstream 2: traversal changes
3. Workstream 3: split/rebalance/merge
4. Workstream 4: create-table/create-index/metadata/ROWID
5. Workstream 5: secondary indexes and integration tests

Reason:

1. Secondary indexes depend on stable B+Tree leaf semantics.
2. Hidden `ROWID` and metadata are easier once root/page behavior is stable.
3. Tests should be added continuously, but full secondary-index tests should wait until the core tree is reliable.

## Milestones

### Milestone 1: Page Format Ready

1. `is_leaf` added
2. leaf/internal helpers work
3. leaf root initialization works

### Milestone 2: Core B+Tree Traversal Ready

1. `BtreeFirst()` lands on leftmost leaf
2. `BtreeMoveTo()` routes through internal nodes and ends at a leaf
3. `BtreeNext()` follows leaf links

### Milestone 3: Split and Delete Rebalance Ready

1. leaf split works
2. internal split works
3. merge/redistribution works
4. root split/collapse works

### Milestone 4: Table and Index Root Management Ready

1. table creation creates leaf root
2. index creation creates separate leaf root
3. metadata stores/retrieves root references
4. hidden `ROWID` strategy is usable

### Milestone 5: Secondary Index End-to-End Ready

1. index build from table works
2. duplicate secondary keys work
3. two-step lookup works
4. integration tests pass

## Merge Strategy

Recommended branches:

1. `phase2-node-format`
2. `phase2-traversal`
3. `phase2-balance`
4. `phase2-metadata-rowid`
5. `phase2-secondary-index-tests`
6. `phase2-integration`

Recommended merge order:

1. `phase2-node-format`
2. `phase2-traversal`
3. `phase2-balance`
4. `phase2-metadata-rowid`
5. `phase2-secondary-index-tests`

## Team Alignment Decisions Needed Up Front

Before implementation starts, the team should explicitly agree on these items:

1. Exact new `NodePageHeaderByteView` layout.
2. Whether `ZeroPage()` takes a page-type parameter or leafness is set after initialization.
3. Exact routing comparison rule in internal nodes.
4. Exact separator behavior for leaf split vs internal split.
5. Row locator format for secondary indexes.
6. Minimal metadata strategy for storing extra root page numbers.

## Recommended Technical Choices

To reduce churn, use these defaults unless the team agrees otherwise:

1. Use hidden `ROWID` instead of `(page_number, cell_index)` as the long-term row locator.
2. Use composite secondary index key `(secondary_key, PK_or_ROWID)`.
3. Keep secondary-index payload small.
4. Avoid storing full tuples in secondary-index leaves for this phase.

## Risks

1. `btree_balance.cc` is the highest-risk file because current logic is strongly B-tree-oriented.
2. `BtreeNext()` can appear correct on single-page trees but fail once leaves split.
3. Physical `(page_number, cell_index)` row pointers can break when pages rebalance.
4. Metadata and hidden `ROWID` work can block secondary-index implementation if started too late.

## Weekly Execution Suggestion

### Day 1

1. Finalize interface decisions.
2. Start Workstreams 1 and 2.

### Day 2

1. Finish page-format changes.
2. Integrate basic traversal changes.
3. Add initial traversal tests.

### Day 3

1. Start split/rebalance conversion.
2. Start create-table/create-index updates.

### Day 4

1. Stabilize splits and merges.
2. Add hidden `ROWID` support.
3. Begin secondary-index build/search implementation.

### Day 5

1. Merge all branches into integration branch.
2. Fix regressions.
3. Run full test pass and produce report screenshots/results.

## Final Definition of Done

Phase 2 is complete when:

1. The tree behaves as a B+Tree, not a B-tree.
2. Internal nodes store routing keys only.
3. Leaf nodes store actual payload and are linked.
4. Sequential scans use leaf links.
5. Table and index roots are created separately.
6. Secondary indexes support duplicate secondary keys through composite keys.
7. Hidden `ROWID` exists for tables without a PK.
8. Tests cover both core tree behavior and index behavior.
