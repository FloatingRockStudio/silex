#pragma once

/// @file DAG.h
/// @brief Directed acyclic graph with topological sort for expression dependency resolution.

#include <algorithm>
#include <map>
#include <queue>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace silex {
namespace core {

/// Lightweight directed acyclic graph for expression dependency resolution.
/// Replaces NetworkX usage with ~50 lines of code.
class DAG {
public:
    /// Add a node to the graph.
    void addNode(int node) {
        m_adjacency[node]; // ensure the node exists
        m_inDegree.emplace(node, 0);
    }

    /// Add a directed edge from source to target.
    void addEdge(int source, int target) {
        addNode(source);
        addNode(target);
        m_adjacency[source].insert(target);
        m_inDegree[target]++;
    }

    /// Check if the graph is a valid DAG (no cycles).
    bool isDAG() const {
        auto sorted = topologicalSort();
        return sorted.size() == m_adjacency.size();
    }

    /// Return nodes in topological order. Returns empty if cycles exist.
    std::vector<int> topologicalSort() const {
        std::map<int, int> inDegree = m_inDegree;
        std::queue<int> zeroIn;

        for (const auto& [node, degree] : inDegree) {
            if (degree == 0) {
                zeroIn.push(node);
            }
        }

        std::vector<int> result;
        result.reserve(m_adjacency.size());

        while (!zeroIn.empty()) {
            int node = zeroIn.front();
            zeroIn.pop();
            result.push_back(node);

            auto it = m_adjacency.find(node);
            if (it != m_adjacency.end()) {
                for (int neighbor : it->second) {
                    inDegree[neighbor]--;
                    if (inDegree[neighbor] == 0) {
                        zeroIn.push(neighbor);
                    }
                }
            }
        }

        return result;
    }

    /// Get all nodes in the graph.
    std::vector<int> nodes() const {
        std::vector<int> result;
        result.reserve(m_adjacency.size());
        for (const auto& [node, _] : m_adjacency) {
            result.push_back(node);
        }
        return result;
    }

    /// Get the number of nodes.
    size_t nodeCount() const { return m_adjacency.size(); }

    /// Get the number of edges.
    size_t edgeCount() const {
        size_t count = 0;
        for (const auto& [_, neighbors] : m_adjacency) {
            count += neighbors.size();
        }
        return count;
    }

private:
    std::map<int, std::set<int>> m_adjacency;
    std::map<int, int> m_inDegree;
};

} // namespace core
} // namespace silex
