#ifndef VIRUS_GENEALOGY_H
#define VIRUS_GENEALOGY_H

#include <memory>
#include <map>
#include <set>
#include <vector>
#include <queue>

class VirusAlreadyCreated : public std::exception {
public:
    const char *what() const noexcept override {
        return "VirusAlreadyCreated";
    }
};

class VirusNotFound : public std::exception {
public:
    const char *what() const noexcept override {
        return "VirusNotFound";
    }
};

class TriedToRemoveStemVirus : public std::exception {
public:
    const char *what() const noexcept override {
        return "TriedToRemoveStemVirus";
    }
};

template<class Virus>
class VirusGenealogy {
private:
    class Node;

    using virus_id_t = typename Virus::id_type;
    using smart_ptr = std::shared_ptr<Node>;
    using set_t = std::set<smart_ptr>;

    // Guard used in remove() function when many deletes may happen at once.
    class RemoveGuard {
    private:
        set_t *guarded_set;
        typename set_t::iterator to_be_removed;
        bool rollback;

    public:
        RemoveGuard(set_t *set, smart_ptr node)
                : guarded_set(set), rollback(true) {
            to_be_removed = guarded_set->find(node);
        }

        RemoveGuard(RemoveGuard const &) = delete;

        RemoveGuard &operator=(RemoveGuard const &) = delete;

        // We can erase element when we know that everything went OK
        // inside one remove() call.
        ~RemoveGuard() noexcept {
            if (!rollback)
                guarded_set->erase(to_be_removed);
        }

        void drop_rollback() noexcept {
            rollback = false;
        }
    };

    // Subclass responsible for gathering all information about virus
    // and its connections in virus graph.
    class Node {
    public:
        Virus virus;
        set_t parents, children;

        explicit Node(virus_id_t id) : virus(id) {};

        virus_id_t get_id() {
            return virus.get_id();
        }

        // Basic bidirectional iterator implementation.
        class Iterator {
        public:
            using iterator_category = std::bidirectional_iterator_tag;
            using difference_type = std::ptrdiff_t;
            using value_type = Virus;
            using pointer = const value_type *;
            using reference = const value_type &;

        private:
            typename set_t::iterator ptr;

        public:
            Iterator() {};

            explicit Iterator(typename set_t::iterator p) : ptr(p) {};

            reference operator*() const {
                return (*ptr)->virus;
            }

            pointer operator->() {
                return &((*ptr)->virus);
            }

            Iterator &operator++() {
                ++ptr;
                return *this;
            }

            Iterator operator++(int) {
                Iterator res(*this);
                operator++();
                return res;
            }

            Iterator &operator--() {
                --ptr;
                return *this;
            }

            Iterator operator--(int) {
                Iterator res(*this);
                operator--();
                return res;
            }

            friend bool operator==(Iterator const &a, Iterator const &b) {
                return a.ptr == b.ptr;
            }

            friend bool operator!=(Iterator const &a, Iterator const &b) {
                return !(a == b);
            }
        };

        Iterator begin() {
            return Iterator(children.begin());
        }

        Iterator end() {
            return Iterator(children.end());
        }
    };

    // Viruses are stored by map <id, smart pointer to virus>.
    using virus_map = std::map<virus_id_t, smart_ptr>;
    virus_map viruses;
    smart_ptr stemNode;

    // The connect() function that takes a vector as an argument.
    // It gathers all changes (inserts to sets) and in case of exception
    // it restores them to the beginning state.
    void connect(virus_id_t const &child_id,
                 std::vector<virus_id_t> const &parent_ids) {
        std::vector<std::pair<set_t *, typename set_t::iterator> > in_process;
        try {
            auto child = viruses.at(child_id);
            for (auto &id: parent_ids) {
                auto parent = viruses.at(id);
                if (!parent->children.contains(child)) {
                    auto it = parent->children.insert(child).first;
                    try {
                        in_process.push_back(
                                std::make_pair(&parent->children, it));
                    }
                    catch (...) {
                        parent->children.erase(it);
                        throw;
                    }
                    child->parents.insert(parent);
                }
            }
        } catch (const std::out_of_range &e) {
            for (auto &[set, iter]: in_process)
                set->erase(iter);
            throw VirusNotFound();
        } catch (...) {
            for (auto &[set, iter]: in_process)
                set->erase(iter);
            throw;
        }
    }

public:
    using children_iterator = typename Node::Iterator;

    // Creates new genealogy with stem Virus.
    explicit VirusGenealogy(virus_id_t const &stem_id) {
        stemNode = std::make_shared<Node>(stem_id);
        viruses.emplace(std::make_pair(stem_id, stemNode));
    }

    VirusGenealogy(const VirusGenealogy &) = delete;

    VirusGenealogy &operator=(const VirusGenealogy &) = delete;

    virus_id_t get_stem_id() const {
        return stemNode->get_id();
    }

    ~VirusGenealogy() noexcept {
        for (auto it: viruses) {
            it.second->children.clear();
            it.second->parents.clear();
        }
        viruses.clear();
    }

    // Returns iterator to the beginning of children list of given virus.
    VirusGenealogy<Virus>::children_iterator
    get_children_begin(virus_id_t const &id) const {
        try {
            return viruses.at(id)->begin();
        } catch (const std::out_of_range &e) {
            throw VirusNotFound();
        }
    }

    // Returns iterator to the end of children list of given virus.
    VirusGenealogy<Virus>::children_iterator
    get_children_end(virus_id_t const &id) const {
        try {
            return viruses.at(id)->end();
        } catch (const std::out_of_range &e) {
            throw VirusNotFound();
        }
    }

    // Returns ids of parents of given virus.
    std::vector<virus_id_t>
    get_parents(virus_id_t const &id) const {
        std::vector<virus_id_t> parent_ids;
        try {
            for (auto parent: viruses.at(id)->parents)
                parent_ids.push_back(parent->get_id());
        } catch (const std::out_of_range &e) {
            throw VirusNotFound();
        }

        return parent_ids;
    }

    // Checks if virus with given id exists.
    bool exists(virus_id_t const &id) const {
        return viruses.contains(id);
    }

    // Returns reference to virus with given id.
    const Virus &operator[](virus_id_t const &id) const {
        try {
            return viruses.at(id)->virus;
        } catch (const std::out_of_range &e) {
            throw VirusNotFound();
        }
    }

    // Adds new edge to genealogy graph.
    void connect(virus_id_t const &child_id,
                 virus_id_t const &parent_id) {
        std::vector<virus_id_t> parent_ids{parent_id};
        connect(child_id, parent_ids);
    }

    // Creates virus with new id with one parent.
    void create(virus_id_t const &id,
                virus_id_t const &parent_id) {
        std::vector<virus_id_t> parents{parent_id};
        create(id, parents);
    }

    // Creates virus with new id with many parents.
    void create(virus_id_t const &id,
                std::vector<virus_id_t> const &parent_ids) {
        if (parent_ids.empty())
            return;

        if (exists(id))
            throw VirusAlreadyCreated();

        auto it = viruses.insert({id, std::make_shared<Node>(id)}).first;

        try {
            connect(id, parent_ids);
        } catch (...) {
            viruses.erase(it);
            throw;
        }
    }

    // Remove virus with given id from graph. Takes care about case when
    // deleting one virus cause deletion of other viruses.
    void remove(virus_id_t const &id) {
        if (id == get_stem_id())
            throw TriedToRemoveStemVirus();

        smart_ptr begin_node;
        try {
            begin_node = viruses.at(id);
        }
        catch (const std::out_of_range &e) {
            throw VirusNotFound();
        }

        // Set of nodes which should be removed from graph.
        std::set<smart_ptr> fully_remove;
        std::vector<typename virus_map::iterator> remove_iterators;

        // For each node count number of his parents which were deleted.
        std::map<virus_id_t, size_t> removed_parents;

        // Queue used in bfs-like graph traverse.
        std::queue<smart_ptr> to_process;

        to_process.push(begin_node);
        while (!to_process.empty()) {
            auto &current = to_process.front();
            to_process.pop();
            fully_remove.insert(current);
            remove_iterators.push_back(viruses.find(current->get_id()));
            for (auto child: current->children) {
                auto &val = removed_parents[child->get_id()];
                val++;
                // If all parents were already deleted child should be deleted.
                if (val == child->parents.size())
                    to_process.push(child);
            }
        }

        // Remove guards used to safely erase deleted nodes from parents sets
        // of nodes which stay in graph.
        std::vector<std::unique_ptr<RemoveGuard>> remove_edge;
        for (auto &current: fully_remove) {
            for (auto &child: current->children) {
                if (fully_remove.find(child) == fully_remove.end()) {
                    remove_edge.push_back(std::make_unique<RemoveGuard>
                                                  (&(child->parents), current));
                }
            }
        }

        for (auto &parent: begin_node->parents) {
            remove_edge.push_back(std::make_unique<RemoveGuard>
                                          (&(parent->children), begin_node));
        }

        // No exceptions can occur from now.
        for (auto &remove_guard: remove_edge)
            remove_guard->drop_rollback();
        remove_edge.clear();

        for (auto &it: fully_remove) {
            it->parents.clear();
            it->children.clear();
        }

        for (auto &iter: remove_iterators)
            viruses.erase(iter);
    }

};

#endif //VIRUS_GENEALOGY_H
