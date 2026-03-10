//
// Created by Yiyang Huo on 9/8/23.
//

#include "pager.h"

#include <fstream>
#include <ostream>

#include "gtest/gtest.h"
#include "os.h"

void printFileContents(const std::string &filename) {
  std::ifstream file(filename, std::ios::binary);
  if (file.is_open()) {
    // Read the entire file into a buffer
    std::vector<char> buffer(std::istreambuf_iterator<char>(file), {});
    file.seekg(0, std::ios::beg);  // Reset the file pointer to the beginning
                                   // (for future reads

    // Iterate over the buffer and print only printable characters
    for (const char &byte : buffer) {
      if (std::isprint(byte) || byte == '\n' || byte == '\r') {
        std::cout << byte;
      }
    }

    std::cout << std::endl;  // Flush the line

    file.close();
  } else {
    std::cerr << "Unable to open file " << filename << std::endl;
  }
}


TEST(PagerRollbackTest, TestRollbackAndCommit) {
  std::string filename = "test_TestRollbackAndCommit.db";
  ResultCode rc;
  Pager pager(filename, 10, EvictionPolicy::FIRST_NON_DIRTY);
  rc = pager.SqlitePagerRollback();
  EXPECT_EQ(rc, ResultCode::kOk);
  rc = pager.SqlitePagerCommit();
  EXPECT_EQ(rc, ResultCode::kError);
}

// Test rollback functionality when database corruption is detected.
TEST(PagerRollbackTest, DatabaseCorruptionDuringRollback) {
  std::string filename = "test_DatabaseCorruptionDuringRollback.db";
  std::remove(filename.c_str());
  std::remove((filename + "-journal").c_str());
  Pager pager(filename, 10, EvictionPolicy::FIRST_NON_DIRTY);

  BasePage *p_page = nullptr;

  // Load and modify a page
  ResultCode rc = pager.SqlitePagerGet(1, &p_page, SampleMemPage::create);
  EXPECT_EQ(rc, ResultCode::kOk);
  ASSERT_NE(p_page, nullptr);
  rc = pager.SqlitePagerWrite(p_page);
  EXPECT_EQ(rc, ResultCode::kOk);

  // Simulate corruption by introducing an error
  pager.err_mask_.insert(SqlitePagerError::K_PAGER_ERROR_CORRUPT);

  // Attempt rollback
  rc = pager.SqlitePagerRollback();
  EXPECT_NE(rc, ResultCode::kOk);
}

// rollback TODO part 1 test
// It inserts something into the page
// If student successfully rollback in WriteBack, It will receive an empty page
TEST(PagerRollbackTest, RollbackInWriteLock) {
  // remove test file avoid unstable behavior
  std::string filename = "RollbackWithInWriteLock.db";
  std::remove(filename.c_str());
  std::remove((filename + "-journal").c_str());
  std::remove((filename + "db-checkpoint").c_str());

  {
    Pager pager(filename, 10, EvictionPolicy::FIRST_NON_DIRTY);
    BasePage *p1 = nullptr;
    // get first page. Create a new one if not exist
    auto rc =
        pager.SqlitePagerGet(1, &p1, []() { return SampleMemPage::create(); });
    ASSERT_EQ(rc, ResultCode::kOk) << "RollbackInWriteLock Test SqlitePagerGet Fail\n";

    // Write RollbackInWriteLock into the page
    std::memcpy(p1->p_image_->data(), "RollbackInWriteLock", 19);
    rc = pager.SqlitePagerWrite(p1);
    ASSERT_EQ(rc, ResultCode::kOk)<< "RollbackInWriteLock Test SqlitePagerWrite Fail\n";

    // make sure page is under write lock and the journal is opened
    ASSERT_EQ(pager.lock_state_, SqliteLockState::K_SQLITE_WRITE_LOCK)<< "RollbackInWriteLock Test Insert Error Fail\n";
    ASSERT_TRUE(pager.is_journal_open_);

    // insert error for rollback
    pager.err_mask_.insert(SqlitePagerError::K_PAGER_ERROR_MEM);
    pager.SqlitePagerRollback();
  }
  {
    // create new pager read same file to avoid mem error
    Pager pager2(filename, 10, EvictionPolicy::FIRST_NON_DIRTY);
    BasePage *p1_check = nullptr;
    auto rc2 = pager2.SqlitePagerGet(1, &p1_check,
                                     []() { return SampleMemPage::create(); });
    ASSERT_EQ(rc2, ResultCode::kOk) << "Failed Rollback In Write Lock";
    // get stored value in the page
    std::string content_after(
        reinterpret_cast<char *>(p1_check->p_image_->data()), 19);
    EXPECT_NE(content_after, "RollbackInWriteLock")
        << "Failed Rollback In Write Lock";
  }
}


TEST(PagerGetTest, TestFetchOnePage) {
  std::string filename = "test_TestFetchOnePage.db";
  std::string journal_filename = "test_TestFetchOnePage.db-journal";
  // Delete these 2 files
  std::remove(filename.c_str());
  std::remove(journal_filename.c_str());
  ResultCode rc;
  Pager pager(filename, 10, EvictionPolicy::FIRST_NON_DIRTY);
  BasePage *p_base_page = nullptr;
  rc = pager.SqlitePagerGet(1, &p_base_page, SampleMemPage::create);
  rc = pager.SqlitePagerGet(2, &p_base_page, SampleMemPage::create);
  rc = pager.SqlitePagerGet(3, &p_base_page, SampleMemPage::create);
  rc = pager.SqlitePagerLookup(1, &p_base_page);
  ASSERT_NE(p_base_page, nullptr);
  rc = pager.SqlitePagerWrite(p_base_page);
  EXPECT_EQ(rc, ResultCode::kOk);
}

// Verify that a page can be fetched and created if not in the cache.
TEST(PagerGetTest, BasicPageFetch) {
  std::string filename = "test_BasicPageFetch.db";
  std::remove(filename.c_str());
  Pager pager(filename, 10, EvictionPolicy::FIRST_NON_DIRTY);

  BasePage *p_page = nullptr;
  ResultCode rc = pager.SqlitePagerGet(1, &p_page, SampleMemPage::create);
  EXPECT_EQ(rc, ResultCode::kOk);
  ASSERT_NE(p_page, nullptr);
  // Verify page number
  EXPECT_EQ(pager.SqlitePagerPageNumber(p_page), 1);

  // Check the content of the newly created page (should be zero-initialized)
  std::vector<std::byte> expected_content(1024, std::byte(0));
  EXPECT_EQ(std::memcmp(p_page->p_image_->data(), expected_content.data(),
                        expected_content.size()),
            0);
}

TEST(PagerPageCountTest, DatabaseFileEmpty) {
  std::string filename = "test_DatabaseFileEmpty.db";
  std::string journal_filename = "test_DatabaseFileEmpty.db-journal";
  // Delete these 2 files
  std::remove(filename.c_str());
  std::remove(journal_filename.c_str());
  ResultCode rc;
  Pager pager(filename, 10, EvictionPolicy::FIRST_NON_DIRTY);
  BasePage *p_base_page = nullptr;
  u32 page_count;
  // There is nothing in the database file
  // So the page count should be 0
  page_count = pager.SqlitePagerPageCount();
  EXPECT_EQ(page_count, 0);
  // Load one page into the cache
  rc = pager.SqlitePagerGet(1, &p_base_page, SampleMemPage::create);
  EXPECT_EQ(rc, ResultCode::kOk);
  // Since nothing is written into the database file, the page count should
  // still be 0
  page_count = pager.SqlitePagerPageCount();
  EXPECT_EQ(page_count, 0);
}

TEST(PagerJournalTest, HandlesJournalFilePlayback) {
  // Arrange
  std::string filename = "test_HandlesJournalFilePlayback.db";
  std::string journal_filename = "test_HandlesJournalFilePlayback.db-journal";
  std::remove(filename.c_str());
  std::remove(journal_filename.c_str());
  ResultCode rc;
  Pager pager(filename, 10, EvictionPolicy::FIRST_NON_DIRTY);
  BasePage *p_base_page = nullptr;
  rc = pager.SqlitePagerGet(1, &p_base_page, SampleMemPage::create);
  EXPECT_EQ(rc, ResultCode::kOk);
  ASSERT_NE(p_base_page, nullptr);
  rc = pager.SqlitePagerUnref(p_base_page);
  EXPECT_EQ(rc, ResultCode::kOk);
  rc = pager.SqlitePagerGet(1, &p_base_page, SampleMemPage::create);
  EXPECT_EQ(rc, ResultCode::kOk);
}

// Write data to multiple pages sequentially and ensure the commit operation
// persists all data.
TEST(PagerJournalTest, SequentialWritesAndCommit) {
  std::string filename = "test_SequentialWritesAndCommit.db";
  std::remove(filename.c_str());
  ResultCode rc;
  Pager pager(filename, 10, EvictionPolicy::FIRST_NON_DIRTY);
  BasePage *p_base_page = nullptr;

  // Write to pages 1 to 3
  for (int i = 1; i <= 3; ++i) {
    rc = pager.SqlitePagerGet(i, &p_base_page, SampleMemPage::create);
    EXPECT_EQ(rc, ResultCode::kOk);
    ASSERT_NE(p_base_page, nullptr);
    rc = pager.SqlitePagerWrite(p_base_page);
    EXPECT_EQ(rc, ResultCode::kOk);

    // Fill page with data
    std::vector<std::byte> data(1024, std::byte(i));
    std::memcpy(p_base_page->p_image_->data(), data.data(), data.size());
  }

  // Commit transaction
  rc = pager.SqlitePagerCommit();
  EXPECT_EQ(rc, ResultCode::kOk);

  // Verify data persistence
  for (int i = 1; i <= 3; ++i) {
    rc = pager.SqlitePagerGet(i, &p_base_page, SampleMemPage::create);
    EXPECT_EQ(rc, ResultCode::kOk);
    ASSERT_NE(p_base_page, nullptr);
    std::vector<std::byte> expected_data(1024, std::byte(i));
    EXPECT_EQ(std::memcmp(p_base_page->p_image_->data(), expected_data.data(),
                          expected_data.size()),
              0);
  }
}

// Test for SqlitePagerCommit basic functionality
TEST(PagerCommitTest, TestBasicCommitFunctionality) {
  std::string filename = "test_SqlitePagerCommit.db";
  std::string journal_filename = "test_SqlitePagerCommit.db-journal";
  std::remove(filename.c_str());
  std::remove(journal_filename.c_str());

  // Create a Pager instance and modify a page
  Pager pager(filename, 10, EvictionPolicy::FIRST_NON_DIRTY);
  BasePage *p_page = nullptr;
  ResultCode rc = pager.SqlitePagerGet(1, &p_page, SampleMemPage::create);
  EXPECT_EQ(rc, ResultCode::kOk) << "CHECKPOINT 1: Unexpected result in SqlitePagerGet\n";
  ASSERT_NE(p_page, nullptr) << "CHECKPOINT 1: Page retrieval failed\n";

  // Begin a transaction
  rc = pager.SqlitePagerBegin(p_page);
  EXPECT_EQ(rc, ResultCode::kOk) << "CHECKPOINT 2: SqlitePagerBegin failed\n";

  // Modify the page
  rc = pager.SqlitePagerWrite(p_page);
  EXPECT_EQ(rc, ResultCode::kOk) << "CHECKPOINT 3: SqlitePagerWrite failed\n";
  std::vector<std::byte> test_data = {std::byte(0x54), std::byte(0x65),
                                      std::byte(0x73), std::byte(0x74)};
  std::memcpy(p_page->p_image_->data(), test_data.data(), test_data.size());

  // Verify page is marked as writable and modified in memory
  EXPECT_TRUE(pager.SqlitePagerIsWritable(p_page)) << "CHECKPOINT 4: Page not marked as writable\n";
  EXPECT_EQ(
      std::memcmp(p_page->p_image_->data(), test_data.data(), test_data.size()),
      0) << "CHECKPOINT 5: Data not properly written to memory\n";

  // Verify journal file exists before commit
  {
    std::ifstream check_journal(journal_filename);
    EXPECT_TRUE(check_journal.good()) << "CHECKPOINT 6: Journal file not found before commit\n";
  }

  // Commit transaction
  rc = pager.SqlitePagerCommit();
  EXPECT_EQ(rc, ResultCode::kOk) << "CHECKPOINT 7: SqlitePagerCommit failed\n";

  // Verify journal file is deleted
  {
    std::ifstream check_journal(journal_filename);
    EXPECT_FALSE(check_journal.good()) << "CHECKPOINT 8: Journal file still exists after commit\n";
  }

  // Verify data was persisted to disk
  BasePage *p_new_page = nullptr;
  Pager new_pager(filename, 10, EvictionPolicy::FIRST_NON_DIRTY);
  rc = new_pager.SqlitePagerGet(1, &p_new_page, SampleMemPage::create);
  EXPECT_EQ(rc, ResultCode::kOk) << "CHECKPOINT 9: Failed to get page from new pager\n";
  ASSERT_NE(p_new_page, nullptr) << "CHECKPOINT 9: New page retrieval failed\n";

  // Verify the changes were persisted
  EXPECT_EQ(std::memcmp(p_new_page->p_image_->data(), test_data.data(),
                        test_data.size()),
            0) << "CHECKPOINT 10: Data not persisted correctly to disk\n";
}
