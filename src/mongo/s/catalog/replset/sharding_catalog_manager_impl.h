/**
 *    Copyright (C) 2015 MongoDB Inc.
 *
 *    This program is free software: you can redistribute it and/or  modify
 *    it under the terms of the GNU Affero General Public License, version 3,
 *    as published by the Free Software Foundation.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU Affero General Public License for more details.
 *
 *    You should have received a copy of the GNU Affero General Public License
 *    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *    As a special exception, the copyright holders give permission to link the
 *    code of portions of this program with the OpenSSL library under certain
 *    conditions as described in each individual source file and distribute
 *    linked combinations including the program with the OpenSSL library. You
 *    must comply with the GNU Affero General Public License in all respects for
 *    all of the code used other than as permitted herein. If you modify file(s)
 *    with this exception, you may extend this exception to your version of the
 *    file(s), but you are not obligated to do so. If you do not wish to do so,
 *    delete this exception statement from your version. If you delete this
 *    exception statement from all source files in the program, then also delete
 *    it in the license file.
 */

#pragma once

#include <vector>

#include "mongo/s/catalog/sharding_catalog_manager.h"
#include "mongo/s/client/shard_registry.h"
#include "mongo/stdx/mutex.h"

namespace mongo {

class DatabaseType;
class ShardingCatalogClient;

namespace executor {
class TaskExecutor;
}  // namespace executor

/**
 * Implements the catalog manager for writing to replica set config servers.
 */
class ShardingCatalogManagerImpl final : public ShardingCatalogManager {
public:
    ShardingCatalogManagerImpl(ShardingCatalogClient* catalogClient,
                               std::unique_ptr<executor::TaskExecutor> addShardExecutor);
    virtual ~ShardingCatalogManagerImpl();

    /**
     * Safe to call multiple times as long as the calls are externally synchronized to be
     * non-overlapping.
     */
    Status startup() override;

    void shutDown(OperationContext* txn) override;

    StatusWith<std::string> addShard(OperationContext* txn,
                                     const std::string* shardProposedName,
                                     const ConnectionString& shardConnectionString,
                                     const long long maxSize) override;

    void appendConnectionStats(executor::ConnectionPoolStats* stats) override;

private:
    /**
     * Generates a unique name to be given to a newly added shard.
     */
    StatusWith<std::string> _generateNewShardName(OperationContext* txn);

    /**
     * Validates that the specified connection string can serve as a shard server. In particular,
     * this function checks that the shard can be contacted, that it is not already member of
     * another sharded cluster and etc.
     *
     * @param shardRegistry Shard registry to use for getting a targeter to the shard-to-be.
     * @param connectionString Connection string to be attempted as a shard host.
     * @param shardProposedName Optional proposed name for the shard. Can be omitted in which case
     *      a unique name for the shard will be generated from the shard's connection string. If it
     *      is not omitted, the value cannot be the empty string.
     *
     * On success returns a partially initialized ShardType object corresponding to the requested
     * shard. It will have the hostName field set and optionally the name, if the name could be
     * generated from either the proposed name or the connection string set name. The returned
     * shard's name should be checked and if empty, one should be generated using some uniform
     * algorithm.
     */
    StatusWith<ShardType> _validateHostAsShard(OperationContext* txn,
                                               ShardRegistry* shardRegistry,
                                               const ConnectionString& connectionString,
                                               const std::string* shardProposedName);

    /**
     * Runs the listDatabases command on the specified host and returns the names of all databases
     * it returns excluding those named local and admin, since they serve administrative purpose.
     */
    StatusWith<std::vector<std::string>> _getDBNamesListFromShard(
        OperationContext* txn,
        ShardRegistry* shardRegistry,
        const ConnectionString& connectionString);

    /**
     * Runs a command against a "shard" that is not yet in the cluster and thus not present in the
     * ShardRegistry.
     */
    StatusWith<Shard::CommandResponse> _runCommandForAddShard(OperationContext* txn,
                                                              RemoteCommandTargeter* targeter,
                                                              const std::string& dbName,
                                                              const BSONObj& cmdObj);


    //
    // All member variables are labeled with one of the following codes indicating the
    // synchronization rules for accessing them.
    //
    // (M) Must hold _mutex for access.
    // (R) Read only, can only be written during initialization.
    // (S) Self-synchronizing; access in any way from any context.
    //

    stdx::mutex _mutex;

    // Pointer to the ShardingCatalogClient that can be used to read config server data.
    // This pointer is not owned, so it is important that the object it points to continues to be
    // valid for the lifetime of this ShardingCatalogManager.
    ShardingCatalogClient* _catalogClient;  // (R)

    // Executor specifically used for sending commands to servers that are in the process of being
    // added as shards.  Does not have any connection hook set on it, thus it can be used to talk
    // to servers that are not yet in the ShardRegistry.
    std::unique_ptr<executor::TaskExecutor> _executorForAddShard;  // (R)

    // True if shutDown() has been called. False, otherwise.
    bool _inShutdown = false;  // (M)

    // True if startup() has been called.
    bool _started = false;  // (M)
};

}  // namespace mongo
