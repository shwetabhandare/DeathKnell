#include "DiskPcapMoveTest.h"
#include "MockConf.h"
#include "DiskPcapMover.h"
#include "MockDiskPcapMover.h"
#include "FileIO.h"
#include "MockConfSlave.h"
#include <vector>
#include <g2log.hpp>



TEST_F(DiskPcapMoveTest, GetPcapCaptureFolderPerPartitionLimit) {
   MockConfSlave slave;
   slave.mConfLocation = "resources/test.yaml.MovePcapFiles_Valid";
   Conf conf1 = slave.GetConf();
   EXPECT_EQ(1, conf1.GetPcapCaptureFolderPerPartitionLimit());
   
   slave.mConfLocation = "resources.test.yaml.MovePcapFiles_Invalid";
   Conf conf2 = slave.GetConf();
   EXPECT_EQ(16000, conf2.GetPcapCaptureFolderPerPartitionLimit());
}


TEST_F(DiskPcapMoveTest, DirectoryExistsUpToLimit) {
   MockConf conf;
   conf.mPcapCaptureFolderPerPartitionLimit = 0; // crazy value with zero. This funky stuff is prevented in the Conf
   
   MockDiskPcapMover mover(conf);
   EXPECT_TRUE(mover.DirectoryExistsUpToLimit(mTestDirectory)); 
   
   
   conf.mPcapCaptureFolderPerPartitionLimit = 1;
   EXPECT_FALSE(mover.DirectoryExistsUpToLimit(mTestDirectory)); 

   CreateSubDirectory("1");
   EXPECT_FALSE(mover.DirectoryExistsUpToLimit(mTestDirectory));  // wrong start number
   
   conf.mPcapCaptureFolderPerPartitionLimit = 2;
   CreateSubDirectory("0");
   EXPECT_TRUE(mover.DirectoryExistsUpToLimit(mTestDirectory));  
}





TEST_F(DiskPcapMoveTest, DirectoryNoOverflow) {
   MockConf conf;
   conf.mPcapCaptureFolderPerPartitionLimit = 0;
   
   MockDiskPcapMover mover(conf);
   EXPECT_TRUE(mover.DirectoryNoOverflow(mTestDirectory)); 
      
   CreateSubDirectory("0");
   EXPECT_FALSE(mover.DirectoryNoOverflow(mTestDirectory)); 
   
   conf.mPcapCaptureFolderPerPartitionLimit = 1;
   EXPECT_TRUE(mover.DirectoryNoOverflow(mTestDirectory)); 

   CreateSubDirectory("2");
   EXPECT_FALSE(mover.DirectoryNoOverflow(mTestDirectory)); 

   conf.mPcapCaptureFolderPerPartitionLimit = 2;
   EXPECT_FALSE(mover.DirectoryNoOverflow(mTestDirectory)); 

   CreateSubDirectory("1");
   EXPECT_FALSE(mover.DirectoryNoOverflow(mTestDirectory)); 

   
   CreateSubDirectory("15999");
   EXPECT_FALSE(mover.DirectoryNoOverflow(mTestDirectory)); 
   conf.mPcapCaptureFolderPerPartitionLimit = 16000;
   EXPECT_TRUE(mover.DirectoryNoOverflow(mTestDirectory)); 
}


TEST_F(DiskPcapMoveTest, DoesDirectoryHaveContent_CheckFile) {
   MockConf conf;
   MockDiskPcapMover mover(conf);
   
   EXPECT_FALSE(mover.DoesDirectoryHaveContent(mTestDirectory));
   CreateFile(mTestDirectory, "some_file");  
   EXPECT_TRUE(mover.DoesDirectoryHaveContent(mTestDirectory));
}


TEST_F(DiskPcapMoveTest, CleanDirectoryOfFileContents_BogusDirectory) {
   MockConf conf;
   MockDiskPcapMover mover(conf);
   std::vector<std::string> newDirectories;
   size_t removedFiles{0};   
   EXPECT_FALSE(mover.CleanDirectoryOfFileContents("", removedFiles, newDirectories));
   EXPECT_FALSE(mover.CleanDirectoryOfFileContents("/does/not/exist/", removedFiles, newDirectories));
}


TEST_F(DiskPcapMoveTest, CleanDirectoryOfFileContents) {
   MockConf conf;
   MockDiskPcapMover mover(conf);
   
   std::vector<std::string> newDirectories;
   size_t removedFiles{0};  
   CreateSubDirectory("some_directory");
   EXPECT_TRUE(FileIO::DoesDirectoryExist({mTestDirectory + "/some_directory"}));
   EXPECT_EQ(removedFiles, 0);
   
   CreateFile(mTestDirectory, "some_file");  
   EXPECT_TRUE(mover.CleanDirectoryOfFileContents(mTestDirectory, removedFiles, newDirectories));
   EXPECT_EQ(removedFiles, 1);
   ASSERT_EQ(newDirectories.size(), 1);
   EXPECT_EQ(std::string{mTestDirectory + "/some_directory"}, newDirectories[0]);
   // directories are not removed
   EXPECT_TRUE(FileIO::DoesDirectoryExist({mTestDirectory + "/some_directory"})); 
}


TEST_F(DiskPcapMoveTest,  RemoveEmptyDirectories_FakeDirectoriesAreIgnored) {
      
   MockConf conf;
   MockDiskPcapMover mover(conf);
   
   EXPECT_TRUE(mover.RemoveEmptyDirectories({})); // invalids are ignored
   EXPECT_TRUE(mover.RemoveEmptyDirectories({{""}})); // invalids are ignored

   std::vector<std::string> doNotExist({{}, {" "}, {"/does/not/exist"}});
   EXPECT_TRUE(mover.RemoveEmptyDirectories(doNotExist)); // invalids are ignored
   
   CreateSubDirectory("some_directory");
   std::string real = {mTestDirectory+"/"+"some_directory"};
   EXPECT_TRUE(FileIO::DoesDirectoryExist(real));
   EXPECT_TRUE(mover.RemoveEmptyDirectories({{"does_not_exist"},real})); // the one invalid is ignored
   
}

TEST_F(DiskPcapMoveTest, RemoveDirectories__ExpectNonEmptyToStay) {
   MockConf conf;
   MockDiskPcapMover mover(conf);
   CreateSubDirectory("some_directory");
   CreateFile({mTestDirectory + "/some_directory/"}, "some_file");
   EXPECT_TRUE(FileIO::DoesFileExist({mTestDirectory + "/some_directory/some_file"}));
   EXPECT_FALSE(mover.RemoveEmptyDirectories({{mTestDirectory + "/some_directory"}}));
   EXPECT_TRUE(FileIO::DoesFileExist({mTestDirectory + "/some_directory/some_file"}));   
}


TEST_F(DiskPcapMoveTest, CreatePcapSubDirectories_FailForBogusLocation){
   MockConf conf;
   MockDiskPcapMover mover(conf);
   EXPECT_FALSE(mover.CreatePcapSubDirectories({"does/not/exist"}));
}


TEST_F(DiskPcapMoveTest, CreateDirectories__ThenDeleteThem) {
   MockConf conf;
   MockDiskPcapMover mover(conf);
   
   conf.mPcapCaptureFolderPerPartitionLimit = 10;
   mover.CreatePcapSubDirectories(mTestDirectory);
   std::vector<std::string> all;
   for (size_t index = 0; index < 10; ++index) {
      std::string directory{mTestDirectory + "/" + std::to_string(index)};
      ASSERT_TRUE(FileIO::DoesDirectoryExist(directory)); // skip annoying failures if one fails
      all.push_back(directory);
   }
   
   EXPECT_TRUE(mover.RemoveEmptyDirectories(all));
   size_t count = 0;
   for (const auto& dir: all) {
      ASSERT_FALSE(FileIO::DoesDirectoryExist(dir));
      ++count;
   }
   EXPECT_EQ(count, 10); 
}



// 
// Here starts High Level Pcap Storage validation, cleanup and setup tests
// 
//
TEST_F(DiskPcapMoveTest, IsPcapStorageValid__Invalid) {

   MockConf conf;
   MockDiskPcapMover mover(conf);
   EXPECT_FALSE(mover.IsPcapStorageValid("/blah/DoesNotExist/123/"));
}


TEST_F(DiskPcapMoveTest, IsPcapStorageValid) {

   MockConf conf;
   MockDiskPcapMover mover(conf);
   conf.mPcapCaptureFolderPerPartitionLimit = 10;
   
   for (size_t index = 0; index < (conf.mPcapCaptureFolderPerPartitionLimit -1); ++index) {
      CreateSubDirectory(std::to_string(index));
   }
   EXPECT_FALSE(mover.IsPcapStorageValid(mTestDirectory));
   
   // Only exactly the right configuration will be accepted as valid
   CreateSubDirectory(std::to_string(9));
   EXPECT_TRUE(mover.IsPcapStorageValid(mTestDirectory));
   
   CreateSubDirectory(std::to_string(11));
   EXPECT_FALSE(mover.IsPcapStorageValid(mTestDirectory));
}


TEST_F(DiskPcapMoveTest, IsPcapStorageValid_loose_files) {

   MockConf conf;
   MockDiskPcapMover mover(conf);
   conf.mPcapCaptureFolderPerPartitionLimit = 10;
   
   for (size_t index = 0; index < (conf.mPcapCaptureFolderPerPartitionLimit); ++index) {
      CreateSubDirectory(std::to_string(index));
   }
   EXPECT_TRUE(mover.IsPcapStorageValid(mTestDirectory));
   
   CreateFile(mTestDirectory, "some_file");
   EXPECT_FALSE(mover.IsPcapStorageValid(mTestDirectory));
}

TEST_F(DiskPcapMoveTest, DeleteOldPcapStorage) {
   MockConf conf;
   MockDiskPcapMover mover(conf);
   mover.mOverrideProbePcapOriginalLocation = true; // using GetFirstPcapCaptureLocation from MockConf

   conf.mPcapCaptureFolderPerPartitionLimit = 10;
   conf.mOverrideGetPcapCaptureLocations = true;
   conf.mPCapCaptureLocations.clear();
   conf.mPCapCaptureLocations.push_back(mTestDirectory);
   //   // Only do this test if the setup is OK. Otherwise it would be 
   //   // really, really, really bad
   ASSERT_EQ(mover.GetOriginalProbePcapLocation(), mTestDirectory);
   ASSERT_EQ(conf.GetFirstPcapCaptureLocation(), mTestDirectory);
   ASSERT_EQ(conf.GetPcapCaptureLocations().size(), 1);
   LOG(INFO) << "TEST_F(DiskPcapMoveTest, DeleteOldPcapStorage)";
   EXPECT_TRUE(mover.DeleteOldPcapStorage());
   
   CreateFile(mTestDirectory, "some_file_1");
   CreateSubDirectory("some_directory");
   CreateFile({mTestDirectory + "/some_directory"}, "some_file_2");
   EXPECT_TRUE(mover.DeleteOldPcapStorage());
}


TEST_F(DiskPcapMoveTest, IsPcapStorageSetupAlready) {
   MockConf conf;
   MockDiskPcapMover mover(conf);
   mover.mOverrideProbePcapOriginalLocation = true; // using GetFirstPcapCaptureLocation from MockConf

   conf.mPcapCaptureFolderPerPartitionLimit = 10;
   conf.mOverrideGetPcapCaptureLocations = true;
   conf.mPCapCaptureLocations.clear();
   conf.mPCapCaptureLocations.push_back(mTestDirectory);
   
   EXPECT_FALSE(mover.IsPcapStorageSetupAlready());
   for (size_t index = 0; index < 9; ++index) {
      CreateSubDirectory(std::to_string(index));
      EXPECT_FALSE(mover.IsPcapStorageSetupAlready());
   }
   CreateSubDirectory(std::to_string(9));
   EXPECT_TRUE(mover.IsPcapStorageSetupAlready());
}

TEST_F(DiskPcapMoveTest,  CreatePcapStorage) {
   MockConf conf;
   MockDiskPcapMover mover(conf);
   mover.mOverrideProbePcapOriginalLocation = true; // using GetFirstPcapCaptureLocation from MockConf

   conf.mPcapCaptureFolderPerPartitionLimit = 10;
   conf.mOverrideGetPcapCaptureLocations = true;
   conf.mPCapCaptureLocations.clear();
   conf.mPCapCaptureLocations.push_back(mTestDirectory);
   
  EXPECT_FALSE(mover.IsPcapStorageSetupAlready());
  EXPECT_TRUE(mover.CreatePcapStorage());
  EXPECT_TRUE(mover.IsPcapStorageSetupAlready());   
}

TEST_F(DiskPcapMoveTest, HashContentsToPcapBuckets) {
   MockConf conf;
   MockDiskPcapMover mover(conf);
   mover.mOverrideProbePcapOriginalLocation = true; // using GetFirstPcapCaptureLocation from MockConf

   conf.mPcapCaptureFolderPerPartitionLimit = 2;
   conf.mOverrideGetPcapCaptureLocations = true;
   conf.mPCapCaptureLocations.clear();
   conf.mPCapCaptureLocations.push_back(mTestDirectory);
   
   EXPECT_TRUE(mover.CreatePcapStorage());
   // nothing to do
   for (const auto& location: conf.GetPcapCaptureLocations()) {
      ASSERT_TRUE(location.find("/tmp/") != std::string::npos);
      EXPECT_TRUE(mover.HashContentsToPcapBuckets(location)) << location;  
   }
   
   CreateFile(mTestDirectory, "553c367a-f638-457c-9916-624e189702ef"); 
   CreateFile(mTestDirectory, "aa681de2-d32b-4aa4-abe0-5b57d47da5de"); 
   for (const auto& location: conf.GetPcapCaptureLocations()) {
      ASSERT_TRUE(location.find("/tmp/") != std::string::npos);
      EXPECT_TRUE(mover.HashContentsToPcapBuckets(location)) << location;  
   }
   
   EXPECT_TRUE(FileIO::DoesFileExist({mTestDirectory + "/0/553c367a-f638-457c-9916-624e189702ef"}));
   EXPECT_TRUE(FileIO::DoesFileExist({mTestDirectory + "/1/aa681de2-d32b-4aa4-abe0-5b57d47da5de"}));  

   // All is already hashed. Nothing to do
   for (const auto& location: conf.GetPcapCaptureLocations()) {
      ASSERT_TRUE(location.find("/tmp/") != std::string::npos);
      EXPECT_TRUE(mover.HashContentsToPcapBuckets(location)) << location;  
   }
   EXPECT_TRUE(FileIO::DoesFileExist({mTestDirectory + "/0/553c367a-f638-457c-9916-624e189702ef"}));
   EXPECT_TRUE(FileIO::DoesFileExist({mTestDirectory + "/1/aa681de2-d32b-4aa4-abe0-5b57d47da5de"}));  
}



TEST_F(DiskPcapMoveTest,TheWholeShebang__NothingCreated) {
   MockConf conf;
   MockDiskPcapMover mover(conf);
   mover.mOverrideProbePcapOriginalLocation = true; // using GetFirstPcapCaptureLocation from MockConf

   conf.mPcapCaptureFolderPerPartitionLimit = 2;
   conf.mOverrideGetPcapCaptureLocations = true;
   conf.mPCapCaptureLocations.clear();
   conf.mPCapCaptureLocations.push_back(mTestDirectory);
   
   // scenario 1: empty with the directories
   EXPECT_FALSE(mover.IsPcapStorageSetupAlready());
   EXPECT_TRUE(mover.CreatePcapStorage());
   EXPECT_TRUE(mover.IsPcapStorageSetupAlready());
}

TEST_F(DiskPcapMoveTest,TheWholeShebang__FilesButNoDirectories) {
   MockConf conf;
   MockDiskPcapMover mover(conf);
   mover.mOverrideProbePcapOriginalLocation = true; // using GetFirstPcapCaptureLocation from MockConf

   conf.mPcapCaptureFolderPerPartitionLimit = 2;
   conf.mOverrideGetPcapCaptureLocations = true;
   conf.mPCapCaptureLocations.clear();
   conf.mPCapCaptureLocations.push_back(mTestDirectory);
   
   // scenario 1: empty with the directories
   EXPECT_FALSE(mover.IsPcapStorageSetupAlready());
   EXPECT_TRUE(mover.CreatePcapStorage());
   
   CreateFile(mTestDirectory, "553c367a-f638-457c-9916-624e189702ef"); 
   CreateFile(mTestDirectory, "aa681de2-d32b-4aa4-abe0-5b57d47da5de"); 
   EXPECT_FALSE(mover.IsPcapStorageSetupAlready());
   EXPECT_TRUE(mover.DiskPcapMover::FixUpPcapStorage());
   EXPECT_TRUE(mover.IsPcapStorageSetupAlready());
}