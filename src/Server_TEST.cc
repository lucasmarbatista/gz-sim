/*
 * Copyright (C) 2018 Open Source Robotics Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
*/

#include <gtest/gtest.h>
#include <csignal>
#include <vector>
#include <ignition/common/Console.hh>
#include <ignition/common/Util.hh>
#include <ignition/transport/Node.hh>

#include "ignition/gazebo/Entity.hh"
#include "ignition/gazebo/EntityComponentManager.hh"
#include "ignition/gazebo/System.hh"
#include "ignition/gazebo/SystemLoader.hh"
#include "ignition/gazebo/Server.hh"
#include "ignition/gazebo/Types.hh"
#include "ignition/gazebo/test_config.hh"

#include "plugins/MockSystem.hh"

using namespace ignition;
using namespace std::chrono_literals;

class ServerFixture : public ::testing::TestWithParam<int>
{
  protected: virtual void SetUp()
  {
    // Augment the system plugin path.  In SetUp to avoid test order issues.
    setenv("IGN_GAZEBO_SYSTEM_PLUGIN_PATH",
           (std::string(PROJECT_BINARY_PATH) + "/lib").c_str(), 1);
  }
};

/////////////////////////////////////////////////
TEST_P(ServerFixture, DefaultServerConfig)
{
  ignition::gazebo::ServerConfig serverConfig;
  gazebo::Server server(serverConfig);
  EXPECT_FALSE(server.Running());
  EXPECT_FALSE(*server.Running(0));
  EXPECT_EQ(std::nullopt, server.Running(1));
  EXPECT_TRUE(*server.Paused());
}

/////////////////////////////////////////////////
TEST_P(ServerFixture, SdfServerConfig)
{
  ignition::gazebo::ServerConfig serverConfig;

  serverConfig.SetSdfFile(std::string(PROJECT_SOURCE_PATH) +
      "/test/worlds/shapes.sdf");

  gazebo::Server server(serverConfig);
  EXPECT_FALSE(server.Running());
  EXPECT_FALSE(*server.Running(0));
  EXPECT_TRUE(*server.Paused());
  EXPECT_EQ(0u, *server.IterationCount());
  EXPECT_EQ(14u, *server.EntityCount());
  EXPECT_EQ(3u, *server.SystemCount());

  EXPECT_TRUE(server.HasEntity("box"));
  EXPECT_FALSE(server.HasEntity("box", 1));
  EXPECT_TRUE(server.HasEntity("sphere"));
  EXPECT_TRUE(server.HasEntity("cylinder"));
  EXPECT_FALSE(server.HasEntity("bad", 0));
  EXPECT_FALSE(server.HasEntity("bad", 1));
}

/////////////////////////////////////////////////
TEST_P(ServerFixture, RunBlocking)
{
  gazebo::Server server;
  EXPECT_FALSE(server.Running());
  EXPECT_FALSE(*server.Running(0));
  EXPECT_TRUE(*server.Paused());
  EXPECT_EQ(0u, server.IterationCount());

  // Make the server run fast.
  server.SetUpdatePeriod(1ns);

  uint64_t expectedIters = 0;
  for (uint64_t i = 1; i < 10; ++i)
  {
    EXPECT_FALSE(server.Running());
    EXPECT_FALSE(*server.Running(0));
    server.Run(true, i, false);
    EXPECT_FALSE(server.Running());
    EXPECT_FALSE(*server.Running(0));

    expectedIters += i;
    EXPECT_EQ(expectedIters, *server.IterationCount());
  }
}

/////////////////////////////////////////////////
TEST_P(ServerFixture, RunNonBlockingPaused)
{
  gazebo::Server server;

  // The server should not be running.
  EXPECT_FALSE(server.Running());
  EXPECT_FALSE(*server.Running(0));

  // The simulation runner should not be running.
  EXPECT_FALSE(*server.Running(0));

  // Invalid world index.
  EXPECT_EQ(std::nullopt, server.Running(1));

  EXPECT_TRUE(*server.Paused());
  EXPECT_EQ(0u, *server.IterationCount());

  // Make the server run fast.
  server.SetUpdatePeriod(1ns);

  EXPECT_TRUE(server.Run(false, 100, true));
  EXPECT_TRUE(*server.Paused());

  EXPECT_TRUE(server.Running());

  // Add a small sleep because the non-blocking Run call causes the
  // simulation runner to start asynchronously.
  IGN_SLEEP_MS(500);
  EXPECT_TRUE(*server.Running(0));

  EXPECT_EQ(0u, server.IterationCount());

  // Attempt to unpause an invalid world
  EXPECT_FALSE(server.SetPaused(false, 1));

  // Unpause the existing world
  EXPECT_TRUE(server.SetPaused(false, 0));

  EXPECT_FALSE(*server.Paused());
  EXPECT_TRUE(server.Running());

  while (*server.IterationCount() < 100)
    IGN_SLEEP_MS(100);

  EXPECT_EQ(100u, *server.IterationCount());
  EXPECT_FALSE(server.Running());
  EXPECT_FALSE(*server.Running(0));
}

/////////////////////////////////////////////////
TEST_P(ServerFixture, RunNonBlocking)
{
  gazebo::Server server;
  EXPECT_FALSE(server.Running());
  EXPECT_FALSE(*server.Running(0));
  EXPECT_EQ(0u, *server.IterationCount());

  // Make the server run fast.
  server.SetUpdatePeriod(1ns);

  server.Run(false, 100, false);
  while (*server.IterationCount() < 100)
    IGN_SLEEP_MS(100);

  EXPECT_EQ(100u, *server.IterationCount());
  EXPECT_FALSE(server.Running());
  EXPECT_FALSE(*server.Running(0));
}

/////////////////////////////////////////////////
TEST_P(ServerFixture, RunNonBlockingMultiple)
{
  gazebo::Server server;
  EXPECT_FALSE(server.Running());
  EXPECT_FALSE(*server.Running(0));
  EXPECT_EQ(0u, *server.IterationCount());

  EXPECT_TRUE(server.Run(false, 100, false));
  EXPECT_FALSE(server.Run(false, 100, false));

  while (*server.IterationCount() < 100)
    IGN_SLEEP_MS(100);

  EXPECT_EQ(100u, *server.IterationCount());
  EXPECT_FALSE(server.Running());
  EXPECT_FALSE(*server.Running(0));
}

/////////////////////////////////////////////////
TEST_P(ServerFixture, SigInt)
{
  gazebo::Server server;
  EXPECT_FALSE(server.Running());
  EXPECT_FALSE(*server.Running(0));

  // Run forever, non-blocking.
  server.Run(false, 0, false);

  IGN_SLEEP_MS(500);

  EXPECT_TRUE(server.Running());
  EXPECT_TRUE(*server.Running(0));

  std::raise(SIGTERM);

  EXPECT_FALSE(server.Running());
  EXPECT_FALSE(*server.Running(0));
}

/////////////////////////////////////////////////
TEST_P(ServerFixture, TwoServersNonBlocking)
{
  ignition::common::Console::SetVerbosity(4);
  gazebo::Server server1;
  gazebo::Server server2;
  EXPECT_FALSE(server1.Running());
  EXPECT_FALSE(*server1.Running(0));
  EXPECT_FALSE(server2.Running());
  EXPECT_FALSE(*server2.Running(0));
  EXPECT_EQ(0u, *server1.IterationCount());
  EXPECT_EQ(0u, *server2.IterationCount());

  // Make the servers run fast.
  server1.SetUpdatePeriod(1ns);
  server2.SetUpdatePeriod(1ns);

  // Start non-blocking
  const size_t iters1 = 9999;
  EXPECT_TRUE(server1.Run(false, iters1, false));

  // Expect that we can't start another instance.
  EXPECT_FALSE(server1.Run(true, 10, false));

  // It's okay to start another server
  EXPECT_TRUE(server2.Run(false, 500, false));

  while (*server1.IterationCount() < iters1 || *server2.IterationCount() < 500)
    IGN_SLEEP_MS(100);

  EXPECT_EQ(iters1, *server1.IterationCount());
  EXPECT_EQ(500u, *server2.IterationCount());
  EXPECT_FALSE(server1.Running());
  EXPECT_FALSE(*server1.Running(0));
  EXPECT_FALSE(server2.Running());
  EXPECT_FALSE(*server2.Running(0));
}

/////////////////////////////////////////////////
TEST_P(ServerFixture, TwoServersMixedBlocking)
{
  gazebo::Server server1;
  gazebo::Server server2;
  EXPECT_FALSE(server1.Running());
  EXPECT_FALSE(*server1.Running(0));
  EXPECT_FALSE(server2.Running());
  EXPECT_FALSE(*server2.Running(0));
  EXPECT_EQ(0u, *server1.IterationCount());
  EXPECT_EQ(0u, *server2.IterationCount());

  // Make the servers run fast.
  server1.SetUpdatePeriod(1ns);
  server2.SetUpdatePeriod(1ns);

  server1.Run(false, 10, false);
  server2.Run(true, 1000, false);

  while (*server1.IterationCount() < 10)
    IGN_SLEEP_MS(100);

  EXPECT_EQ(10u, *server1.IterationCount());
  EXPECT_EQ(1000u, *server2.IterationCount());
  EXPECT_FALSE(server1.Running());
  EXPECT_FALSE(*server1.Running(0));
  EXPECT_FALSE(server2.Running());
  EXPECT_FALSE(*server2.Running(0));
}

/////////////////////////////////////////////////
TEST_P(ServerFixture, AddSystemWhileRunning)
{
  ignition::gazebo::ServerConfig serverConfig;

  serverConfig.SetSdfFile(std::string(PROJECT_SOURCE_PATH) +
      "/test/worlds/shapes.sdf");

  gazebo::Server server(serverConfig);
  EXPECT_FALSE(server.Running());
  EXPECT_FALSE(*server.Running(0));
  server.SetUpdatePeriod(1us);

  // Run the server to test whether we can add system while system is running
  server.Run(false, 0, false);
  EXPECT_EQ(3u, *server.SystemCount());

  gazebo::SystemLoader systemLoader;
  auto mockSystemPlugin = systemLoader.LoadPlugin("libMockSystem.so",
      "ignition::gazebo::MockSystem", nullptr);
  ASSERT_TRUE(mockSystemPlugin.has_value());

  EXPECT_FALSE(*server.AddSystem(mockSystemPlugin.value()));
  EXPECT_EQ(3u, *server.SystemCount());

  // Stop the server
  std::raise(SIGTERM);
}

/////////////////////////////////////////////////
TEST_P(ServerFixture, AddSystemAfterLoad)
{
  ignition::gazebo::ServerConfig serverConfig;

  serverConfig.SetSdfFile(std::string(PROJECT_SOURCE_PATH) +
      "/test/worlds/shapes.sdf");

  gazebo::Server server(serverConfig);
  EXPECT_FALSE(server.Running());
  EXPECT_FALSE(*server.Running(0));

  gazebo::SystemLoader systemLoader;
  auto mockSystemPlugin = systemLoader.LoadPlugin("libMockSystem.so",
      "ignition::gazebo::MockSystem", nullptr);
  ASSERT_TRUE(mockSystemPlugin.has_value());

  EXPECT_EQ(3u, *server.SystemCount());
  EXPECT_TRUE(*server.AddSystem(mockSystemPlugin.value()));
  EXPECT_EQ(4u, *server.SystemCount());

  auto system = mockSystemPlugin.value()->QueryInterface<gazebo::System>();
  EXPECT_NE(system, nullptr);
  gazebo::MockSystem* mockSystem = dynamic_cast<gazebo::MockSystem*>(system);
  EXPECT_NE(mockSystem, nullptr);

  server.SetUpdatePeriod(1us);
  EXPECT_EQ(0u, mockSystem->preUpdateCallCount);
  EXPECT_EQ(0u, mockSystem->updateCallCount);
  EXPECT_EQ(0u, mockSystem->postUpdateCallCount);
  server.Run(true, 1, false);
  EXPECT_EQ(1u, mockSystem->preUpdateCallCount);
  EXPECT_EQ(1u, mockSystem->updateCallCount);
  EXPECT_EQ(1u, mockSystem->postUpdateCallCount);
}

// Run multiple times. We want to make sure that static globals don't cause
// problems.
INSTANTIATE_TEST_CASE_P(ServerRepeat, ServerFixture, ::testing::Range(1, 2));
