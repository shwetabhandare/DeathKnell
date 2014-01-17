#include "ProcessManagerTest.h"
#include "MockProcessManager.h"
#include "MockConf.h"
#include <sstream>
#include <gtest/gtest.h>
#include "g2log.hpp"
#include "ProcessReply.pb.h"
#include <unistd.h>

TEST_F(ProcessManagerTest, StartedWithoutMotherForker) {
   #ifdef LR_DEBUG
   MockConf conf;
   MockProcessManagerNoMotherForker testManager(conf);
   EXPECT_FALSE(testManager.Initialize());
#endif
}
TEST_F(ProcessManagerTest, RegisterDaemonWithEnv) {
#ifdef LR_DEBUG

   MockConf conf;
   std::stringstream testQueue;
   testQueue << "ipc:///tmp/ProcessManagerTest." << getpid();
   conf.mProcessManagmentQueue = testQueue.str();
   MockProcessManager testManager{conf};
   ASSERT_TRUE(testManager.Initialize());
   
   MockProcessManager sendManager(conf);
   ASSERT_TRUE(sendManager.Initialize());
   std::string processName("/bin/sh");
   std::string processArgs;
   EnvironmentMapping env;
   env["a"] = "b";
   processArgs = "resources/sleepIfA.sh";
   pid_t pid;
   EXPECT_TRUE(sendManager.RegisterProcessWithEnvironment(processName, processArgs, env, "",pid));
   processName = "/bin/ps";
   processArgs = "-ef |grep -v grep | grep \"/bin/sh\" | grep ";

   std::stringstream ss;
   ss << pid;
   processArgs += ss.str();
   protoMsg::ProcessReply processReply = sendManager.RunProcess(processName, processArgs);
   std::string result = processReply.result();

   LOG(DEBUG) << result;
   if (std::string::npos == result.find("/bin/sh")) {
      EXPECT_TRUE(false);
   }
   testManager.DeInit();
   sendManager.DeInit();
#endif
}
TEST_F(ProcessManagerTest, RegisterDaemonCleanup) {
#ifdef LR_DEBUG

   MockConf conf;
   std::stringstream testQueue;
   testQueue << "ipc:///tmp/ProcessManagerTest." << getpid();
   conf.mProcessManagmentQueue = testQueue.str();
   MockProcessManager testManager(conf);
   testManager.mRealInit = true;
   ASSERT_TRUE(testManager.Initialize());
   MockProcessManager sendManager(conf);
   ASSERT_TRUE(sendManager.Initialize());
   std::string processName("/bin/sleep");
   std::string processArgs;
   processArgs = "5";
   pid_t pid;
   EXPECT_TRUE(sendManager.RegisterProcess(processName, processArgs, "",pid));
   processName = "/bin/ps";
   processArgs = "-ef | grep \"/bin/sleep\" | grep -v grep | grep ";
   std::stringstream ss;
   ss << pid;
   processArgs += ss.str();

   protoMsg::ProcessReply processReply = sendManager.RunProcess(processName, processArgs);
   std::string result = processReply.result();
   LOG(DEBUG) << result;
   EXPECT_NE(std::string::npos, result.find("/bin/sleep"));
   testManager.DeInit();
   LOG(DEBUG) << "Trying to re-initialize";
   zctx_interrupted = false;
   ASSERT_TRUE(testManager.Initialize());
   processReply = sendManager.RunProcess(processName, processArgs);
   result = processReply.result();
   LOG(DEBUG) << result;
   EXPECT_EQ(std::string::npos, result.find("/bin/sleep"));
   testManager.DeInit();
   sendManager.DeInit();
#endif
}

TEST_F(ProcessManagerTest, RegisterDaemonFails) {
#ifdef LR_DEBUG

   MockConf conf;
   std::stringstream testQueue;
   testQueue << "ipc:///tmp/ProcessManagerTest." << getpid();
   conf.mProcessManagmentQueue = testQueue.str();
   MockProcessManager testManager(conf);
   testManager.mExecFails = true;
   ASSERT_TRUE(testManager.Initialize());
   MockProcessManager sendManager(conf);
   ASSERT_TRUE(sendManager.Initialize());
   std::string processName("/bin/broken");
   std::string processArgs;
   processArgs = "0";
   pid_t pid;
   EXPECT_FALSE(sendManager.RegisterProcess(processName, processArgs, "",pid));
   processName = "/bin/ps";
   processArgs = "-ef | grep \"/bin/sleep\" | grep -v grep | grep ";
   std::stringstream ss;
   ss << pid;
   processArgs += ss.str();
   protoMsg::ProcessReply processReply = sendManager.RunProcess(processName, processArgs);
   std::string result = processReply.result();
   LOG(DEBUG) << result;
   EXPECT_EQ(std::string::npos, result.find("/bin/sleep"));
   testManager.DeInit();
   sendManager.DeInit();
#endif
}

TEST_F(ProcessManagerTest, WriteAndDeletePid) {
   Conf conf;
   MockProcessManager processManager(conf);
   processManager.SetPidDir("/tmp");
   int pid = getpid();
   //the mock moves the run dir to be /tmp so this writes a file /tmp/[id].pid
   EXPECT_TRUE(processManager.WritePid(pid));
   EXPECT_TRUE(processManager.DeletePid(pid));
}

TEST_F(ProcessManagerTest, WritePidThenReclaimAndDelete) {
   Conf conf;
   MockProcessManager processManager(conf);
   processManager.SetPidDir("/tmp");
   protoMsg::ProcessRequest request;
   request.set_realexecstring("ProcessManagerTest");
   request.set_keeprunning(true);
   request.set_path("/fake/path");
   int pid = getpid();
   //the mock moves the run dir to be /tmp so this writes a file /tmp/[id].pid
   EXPECT_TRUE(processManager.WritePid(pid));
   //returns 0 on failure
   EXPECT_NE(0, processManager.ReclaimPid(request, pid));
   EXPECT_TRUE(processManager.DeletePid(pid));
}

TEST_F(ProcessManagerTest, ReclaimNonExistentPid) {
   Conf conf;
   MockProcessManager processManager(conf);
   processManager.SetPidDir("/tmp");
   protoMsg::ProcessRequest request;
   request.set_realexecstring("ProcessManagerTest");
   request.set_keeprunning(true);
   request.set_path("/fake/path");
   int maxPid = 32768;
   //default linux can't have pids above 32768
   EXPECT_EQ(0, processManager.ReclaimPid(request, maxPid + 1));
}

TEST_F(ProcessManagerTest, WritePidThenGetPidsFromFiles) {
   Conf conf;
   MockProcessManager processManager(conf);
   processManager.SetPidDir("/tmp");
   protoMsg::ProcessRequest request;
   request.set_realexecstring("ProcessManagerTest");
   request.set_keeprunning(true);
   request.set_path("/fake/path");
   int pid = getpid();
   //the mock moves the run dir to be /tmp so this writes a file /tmp/[id].pid
   EXPECT_TRUE(processManager.WritePid(pid));
   //returns 0 on failure
   EXPECT_EQ(pid, processManager.GetPidFromFiles(request));
   EXPECT_TRUE(processManager.DeletePid(pid));
}

TEST_F(ProcessManagerTest, WritePidThenGetPidsFromFilesWithOtherPidsInDir) {
   Conf conf;
   MockProcessManager processManager(conf);
   processManager.SetPidDir("/tmp");
   protoMsg::ProcessRequest request;
   request.set_realexecstring("lt-ProcessManagerTest");
   request.set_keeprunning(true);
   request.set_path("/fake/path");
   int pid = getpid();

   //this pid should always exist but not be the process we are looking for
   int initPid = 1;

   int pidBad = pid + 1;
   int pidBad2 = pidBad + 1;
   //the mock moves the run dir to be /tmp so this writes a file /tmp/[id].pid
   EXPECT_TRUE(processManager.WritePid(pid));
   EXPECT_TRUE(processManager.WritePid(initPid));
   EXPECT_TRUE(processManager.WritePid(pidBad));
   EXPECT_TRUE(processManager.WritePid(pidBad2));
   //returns 0 on failure
   EXPECT_EQ(pid, processManager.GetPidFromFiles(request));

   EXPECT_TRUE(processManager.DeletePid(pid));
   //this pid should always exist so it should delete it.
   EXPECT_TRUE(processManager.DeletePid(initPid));
   //GetPidFromFiles should cleanup pids that don't exist but it breaks out
   //if it hits the pid it's looking for. So there may be stale pids in the dir
   processManager.DeletePid(pidBad);
   processManager.DeletePid(pidBad2);
}

TEST_F(ProcessManagerTest, CheckPidTrue) {
   Conf conf;
   MockProcessManager processManager(conf);
   processManager.SetPidDir("/tmp");
   int pid = getpid();
   EXPECT_TRUE(processManager.CheckPidExists(pid));
}

TEST_F(ProcessManagerTest, CheckPidFalse) {
   Conf conf;
   MockProcessManager processManager(conf);
   processManager.SetPidDir("/tmp");
   int maxPid = 32768;
   LOG(INFO) << "checking for pid that can't exist";
   EXPECT_FALSE(processManager.CheckPidExists(maxPid + 1));
}



TEST_F(ProcessManagerTest, ConstructAndInitializeFail) {
#ifdef LR_DEBUG

   MockConf conf;
   conf.mProcessManagmentQueue = "invalid";
   MockProcessManager testManager(conf);
   EXPECT_FALSE(testManager.Initialize());
   testManager.DeInit();

#endif
}

TEST_F(ProcessManagerTest, ConstructAndInitialize) {
#ifdef LR_DEBUG

   MockConf conf;
   std::stringstream testQueue;
   testQueue << "ipc:///tmp/ProcessManagerTest." << getpid();
   conf.mProcessManagmentQueue = testQueue.str();

   MockProcessManager testManager(conf);
   ASSERT_TRUE(testManager.Initialize());
   testManager.DeInit();
   MockProcessManager* testManagerPoint = new MockProcessManager(conf);
   delete testManagerPoint;

#endif
}

TEST_F(ProcessManagerTest, RunProcess) {
#ifdef LR_DEBUG

   MockConf conf;
   std::stringstream testQueue;
   testQueue << "ipc:///tmp/ProcessManagerTest." << getpid();
   conf.mProcessManagmentQueue = testQueue.str();
   MockProcessManager testManager(conf);
   ASSERT_TRUE(testManager.Initialize());
   std::string processName("/bin/ls");
   std::string processArgs;
   processArgs = "-l /bin/ls";
   protoMsg::ProcessReply processReply = testManager.RunProcess(processName, processArgs);
   std::string result = processReply.result();
   EXPECT_NE(std::string::npos, result.find("-rwxr-xr-x. 1 root root"));
   EXPECT_NE(std::string::npos, result.find("/bin/ls"));
   testManager.DeInit();

#endif
}

TEST_F(ProcessManagerTest, RunNonExistantProcess) {
#ifdef LR_DEBUG

   MockConf conf;
   std::stringstream testQueue;
   testQueue << "ipc:///tmp/ProcessManagerTest." << getpid();
   conf.mProcessManagmentQueue = testQueue.str();
   MockProcessManager testManager(conf);
   ASSERT_TRUE(testManager.Initialize());
   std::string processName("/bin/lsssss");
   std::string processArgs;
   processArgs = "-l /bin/ls";
   protoMsg::ProcessReply processReply = testManager.RunProcess(processName, processArgs);
   std::string result = processReply.result();
   EXPECT_TRUE(result.empty());
   testManager.DeInit();

#endif
}

TEST_F(ProcessManagerTest, FailInitializationFromAnotherObject) {
#ifdef LR_DEBUG

   MockConf conf;
   std::stringstream testQueue;
   testQueue << "ipc:///tmp/ProcessManagerTest." << getpid();
   conf.mProcessManagmentQueue = testQueue.str();
   MockProcessManager testManager(conf);
   ASSERT_TRUE(testManager.Initialize());
   conf.mProcessManagmentQueue = "invalid";
   MockProcessManager sendManager(conf);
   EXPECT_FALSE(sendManager.Initialize());

   testManager.DeInit();

#endif
}

TEST_F(ProcessManagerTest, RunProcessFromAnotherObject) {
#ifdef LR_DEBUG

   MockConf conf;
   std::stringstream testQueue;
   testQueue << "ipc:///tmp/ProcessManagerTest." << getpid();
   conf.mProcessManagmentQueue = testQueue.str();
   MockProcessManager testManager(conf);
   ASSERT_TRUE(testManager.Initialize());
   MockProcessManager sendManager(conf);
   ASSERT_TRUE(sendManager.Initialize());
   std::string processName("/bin/ls");
   std::string processArgs;
   processArgs = "-l /bin/ls";
   protoMsg::ProcessReply processReply = sendManager.RunProcess(processName, processArgs);
   std::string result = processReply.result();
   EXPECT_NE(std::string::npos, result.find("-rwxr-xr-x. 1 root root"));
   EXPECT_NE(std::string::npos, result.find("/bin/ls"));
   testManager.DeInit();
   sendManager.DeInit();

#endif
}

TEST_F(ProcessManagerTest, RunNonExistantProcessFromAnotherObject) {
#ifdef LR_DEBUG

   MockConf conf;
   std::stringstream testQueue;
   testQueue << "ipc:///tmp/ProcessManagerTest." << getpid();
   conf.mProcessManagmentQueue = testQueue.str();
   MockProcessManager testManager(conf);
   ASSERT_TRUE(testManager.Initialize());
   MockProcessManager sendManager(conf);
   ASSERT_TRUE(sendManager.Initialize());
   std::string processName("/bin/lsssss");
   std::string processArgs;
   processArgs = "-l /bin/ls";
   protoMsg::ProcessReply processReply = sendManager.RunProcess(processName, processArgs);
   std::string result = processReply.result();
   EXPECT_TRUE(result.empty());
   testManager.DeInit();
   sendManager.DeInit();
#endif
}

//TEST_F(ProcessManagerTest, RegisterDaemon) {
//#ifdef LR_DEBUG
//
//   MockConf conf;
//   std::stringstream testQueue;
//   testQueue << "ipc:///tmp/ProcessManagerTest." << getpid();
//   conf.mProcessManagmentQueue = testQueue.str();
//   ProcessManager testManager(conf);
//   ASSERT_TRUE(testManager.Initialize());
//   ProcessManager sendManager(conf);
//   ASSERT_TRUE(sendManager.Initialize());
//   std::string processName("/bin/sleep");
//   std::string processArgs;
//   processArgs = "4";
//   EXPECT_TRUE(sendManager.RegisterProcess(processName, processArgs, ""));
//   processName = "/bin/ps";
//   processArgs = "-ef | grep \"/bin/sleep\" | grep -v grep | grep ";
//   testQueue.str("");
//   testQueue << getpid();
//   processArgs += testQueue.str();
//   protoMsg::ProcessReply processReply = sendManager.RunProcess(processName, processArgs);
//   std::string result = processReply.result();
//   LOG(DEBUG) << result;
//   EXPECT_NE(std::string::npos, result.find(testQueue.str()));
//   EXPECT_NE(std::string::npos, result.find("/bin/sleep"));
//   std::this_thread::sleep_for(std::chrono::seconds(7));
//   processReply = sendManager.RunProcess(processName, processArgs);
//   std::string result2 = processReply.result();
//   LOG(DEBUG) << result2;
//   EXPECT_NE(std::string::npos, result2.find(testQueue.str()));
//   EXPECT_NE(std::string::npos, result2.find("/bin/sleep"));
//   EXPECT_NE(result, result2);
//   EXPECT_TRUE(sendManager.UnRegisterProcess("/bin/sleep"));
//   processReply = sendManager.RunProcess(processName, processArgs);
//   result2 = processReply.result();
//   LOG(DEBUG) << result2;
//   EXPECT_EQ(std::string::npos, result2.find(testQueue.str()));
//   EXPECT_EQ(std::string::npos, result2.find("/bin/sleep"));
//   testManager.DeInit();
//   sendManager.DeInit();
//#endif
//}




TEST_F(ProcessManagerTest, RegisterDaemonKillFails) {
#ifdef LR_DEBUG

   MockConf conf;
   std::stringstream testQueue;
   testQueue << "ipc:///tmp/ProcessManagerTest." << getpid();
   conf.mProcessManagmentQueue = testQueue.str();
   MockProcessManager testManager(conf);
   testManager.mKillFails = true;
   ASSERT_TRUE(testManager.Initialize());
   MockProcessManager sendManager(conf);
   ASSERT_TRUE(sendManager.Initialize());
   std::string processName("/bin/sleep");
   std::string processArgs;
   processArgs = "5";
   pid_t pid;
   EXPECT_TRUE(sendManager.RegisterProcess(processName, processArgs, "",pid));
   std::this_thread::sleep_for(std::chrono::seconds(1));
   EXPECT_FALSE(sendManager.UnRegisterProcess(processName));
   testManager.DeInit();
   sendManager.DeInit();
#endif
}

TEST_F(ProcessManagerTest, StartProcessBadExitCode) {
#ifdef LR_DEBUG
   MockConf conf;
   MockProcessManager mockProcessManager(conf);
   protoMsg::ProcessRequest request;
   request.set_path("/bin/bash");
   request.set_realexecstring("resources/returnBadExit.sh");
   request.set_args("resources/returnBadExit.sh");
   PidMapping daemonPIDs;
   DaemonRequestMapping requestedDaemons;
   protoMsg::ProcessReply reply;
   mockProcessManager.StartDaemon(request, daemonPIDs, requestedDaemons, reply);
   ASSERT_FALSE(reply.success());
   if (!daemonPIDs.empty()) {
      //Daemon should fail to start
      ASSERT_FALSE(true);
   }
#endif   
}