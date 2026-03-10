#include "node_page.h"

#include "gtest/gtest.h"

TEST(NodePageHeaderByteViewTest, CheckTriviallyCopiable) {
  EXPECT_TRUE(std::is_trivially_copyable_v<NodePageHeaderByteView>);
  EXPECT_TRUE(std::is_trivially_copyable_v<FreeBlockByteView>);
  EXPECT_TRUE(std::is_trivially_copyable_v<CellHeaderByteView>);
}

TEST(CellHeaderByteViewTest, ReturnsCorrectSize) {
  CellHeaderByteView cell_header{};
  cell_header.key_size = 10;
  cell_header.data_size = 20;
  EXPECT_EQ(cell_header.GetCellSize(), 10 + 20 + sizeof(CellHeaderByteView));
}

TEST(AllocateSpaceTest, InsertIntoBlankPage) {
  NodePage tree_page{};
  std::vector<std::byte> key = {std::byte{11}, std::byte{12}, std::byte{13}};
  std::vector<std::byte> data = {std::byte{21}, std::byte{22}, std::byte{23}, std::byte{24}, std::byte{25}};
  Cell cell(key, data);
  tree_page.ZeroPage();
  ImageIndex insert_index = tree_page.AllocateSpace(cell.GetPayloadSize());
  EXPECT_EQ(insert_index, sizeof(NodePageHeaderByteView));
}

TEST(AllocateSpaceTest, InsertTwoCells) {
  NodePage tree_page{};
  std::vector<std::byte> key = {std::byte{11}, std::byte{12}};
  std::vector<std::byte> data = {std::byte{21}};
  Cell cell(key, data);
  tree_page.ZeroPage();
  ImageIndex insert_index = tree_page.AllocateSpace(cell.GetPayloadSize());
  EXPECT_EQ(insert_index, sizeof(NodePageHeaderByteView));
  insert_index = tree_page.AllocateSpace(cell.GetPayloadSize());
  EXPECT_EQ(insert_index, sizeof(NodePageHeaderByteView) + cell.GetPayloadSize());
}

TEST(DestroyExtraTest, NodePageDestroyExtra) {

  // Step 1: Create a NodePage
  NodePage node_page;

  // Step 2: Add a cell to the node page, it will modify cell_trackers, which is part of the extra space
  Cell cell;
  node_page.InsertCell(cell, node_page.GetNumCells());
  EXPECT_EQ(node_page.GetNumCells(), 1);

  // Step 3: Destroy extra space
  node_page.DestroyExtra();

  // Step 4: Now cell_trackers is reset, so the number of cells should be 0
  EXPECT_EQ(node_page.GetNumCells(), 0);
}