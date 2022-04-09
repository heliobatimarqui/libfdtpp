/*------------------------------------------------------------------------------
Copyright (c) 2022 Helio Nunes Santos

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
------------------------------------------------------------------------------*/

#ifndef FDT_PARSER_HPP
#define FDT_PARSER_HPP

#include <cstdint>
#include <cstddef>

// ACCORDING TO DTS SPECIFICATION
#define FDT_MAGIC 0xD00DFEED
#define FDT_BEGIN_NODE 0x00000001
#define FDT_END_NODE 0x00000002
#define FDT_PROP 0x00000003
#define FDT_NOP 0x00000004
#define FDT_END 0x00000009

// RETURN VALUES FOR TRAVERSAL FUNCTION
#define ALL_OK 0
#define INVALID_STRUCTURE_BLOCK -1


namespace fdt {


    // Declarations ---------------------------------------------------------------------------------------------------------------

    struct __attribute__((packed)) fdt_header {
        uint32_t magic;
        uint32_t totalsize;
        uint32_t off_dt_struct;
        uint32_t off_dt_strings;
        uint32_t off_mem_rsvmap;
        uint32_t version;
        uint32_t last_comp_version;
        uint32_t boot_cpuid_phys;
        uint32_t size_dt_strings;
        uint32_t size_dt_struct;
    };

    struct __attribute__((packed)) fdt_prop_desc {
        uint32_t len;
        uint32_t nameoff;
    };

    
    // More likely a namespace than a class...
    class Utilities {
        public:
        static int strcmp(const char* str1, const char* str2);
        static int strncmp(const char* str1, const char* str2, size_t n);
        static size_t strlen(const char* str);
        static const char* strchr(const char* str, char c);
    };

    class TraversalAction {
        protected:
        TraversalAction() = default;
        public:
        virtual void on_FDT_BEGIN_NODE(const fdt_header* header, const uint32_t* token) {}
        virtual void on_FDT_END_NODE(const fdt_header* header, const uint32_t* token) {}
        virtual void on_FDT_PROP_NODE(const fdt_header* header, const uint32_t* token) {}
        virtual void on_FDT_NOP_NODE(const fdt_header* header, const uint32_t* token) {}
        
        virtual bool is_action_satisfied() const { return false; }
    };

    class FdtEngine {
        static const uint32_t* get_aligned_after_offset(const uint32_t* ptr, std::size_t offset);
        static const uint32_t* move_to_next_token(const uint32_t* token_ptr);

        public:
    
        static uint32_t read_value(const uint32_t* ptr);
        static const uint32_t* get_structure_block_ptr(const fdt_header* header);
        static const char* get_string_block_ptr(const fdt_header* header);
        static int traverse_node(const uint32_t*& token_ptr, const fdt_header* header, TraversalAction& action);
    };
    
    class FdtNode {

        protected:
        
        const uint32_t* m_node_token_start = nullptr;
        const fdt_header* m_fdt_header = nullptr;
        
        public:

        FdtNode(const uint32_t* first_token, const fdt_header* fdt_header);
        FdtNode() = default; // For invalid nodes
        ~FdtNode() = default;

        FdtNode get_sub_node(const char* node_name, const char* unit_address = nullptr) const;
        const void* get_property(const char* property_name) const;
        bool has_property(const char* property_name) const;
        virtual bool is_valid() const;
    };
    
    class FDT : public FdtNode {
        
        public:

        FDT(const fdt_header* fdt) {
            // This might not work as expected in some platforms
            uintptr_t ptr_as_uint = reinterpret_cast<uintptr_t>(fdt);

            // The FDT has to be aligned to a 8 byte boundary
            if(ptr_as_uint % 8 == 0) {
                auto magic_number = FdtEngine::read_value(&fdt->magic);
                if(magic_number == FDT_MAGIC) {
                    m_fdt_header = fdt;
                    m_node_token_start = FdtEngine::get_structure_block_ptr(fdt);
                }
            }
        }
     
    };

    class StructValidator : public TraversalAction {
        public:

        virtual void on_FDT_BEGIN_NODE(const fdt_header* header, const uint32_t* token) {}
        virtual void on_FDT_END_NODE(const fdt_header* header, const uint32_t* token) {}
        virtual void on_FDT_PROP_NODE(const fdt_header* header, const uint32_t* token) {}
        virtual void on_FDT_NOP_NODE(const fdt_header* header, const uint32_t* token) {}

    };

    class NodeFinder : public TraversalAction {
        FdtNode m_result;
        const char* m_node_name;
        const char* m_unit_address;  
        
        bool is_same_as(const char* node_string);

        public:
        
        NodeFinder(const char* node_name, const char* unit_address = nullptr);
        
        void on_FDT_BEGIN_NODE(const fdt_header* header, const uint32_t* token) override;
     
        bool is_action_satisfied() const override;

        FdtNode result() const;
    };

    class PropertyFinder : public TraversalAction {
        const void* m_result = nullptr;
        const char* m_looked_property = nullptr;
        uint32_t m_property_length = 0;
        // Given that a property can be empty, we have to flag if it has been found, as when retrieving the content the user might receive a nullptr
        bool m_property_found = false;  
    
        public:

        PropertyFinder(const char* to_look_for) {
            m_looked_property = to_look_for;
        };

        void on_FDT_PROP_NODE(const fdt_header* header, const uint32_t* token) override;
        
        virtual bool is_action_satisfied() const;

        // The user has to be aware on how to interpret the property, given that the data is specific to each property
        const void* property_content() const;
        uint32_t property_length() const;
        bool is_property_found() const;
    };

    //Definitions ---------------------------------------------------------------------------------------------------------------------------

    // Definitions for Utilities 

    int Utilities::strcmp(const char* str1, const char* str2) {
        while(true) {
            auto a = *reinterpret_cast<const unsigned char*>(str1);
            auto b = *reinterpret_cast<const unsigned char*>(str2);
            // If we reach the end of both strings, they are equal
            if(a == 0 && b == 0) {
                return 0;
            }
            else if(a < b) {
                return -1;
            }
            else if(a > b) {
                return 1;
            }

            ++str1;
            ++str2;
        }
    }

    int Utilities::strncmp(const char* str1, const char* str2, size_t n) {
        for(size_t i = 0; i < n; ++i) {
            auto a = *reinterpret_cast<const unsigned char*>(str1 + i);
            auto b = *reinterpret_cast<const unsigned char*>(str2 + i);
        
            if(a == 0 && b == 0) {
                break;
            }
            else if(a < b) {
                return -1;
            }
            else if(a > b) {
                return 1;
            }
        }

        return 0;
    }

    size_t Utilities::strlen(const char* str) {
        size_t i = 0;
        for(;str[i] != '\0'; ++i);
        return i;
    }

    const char* Utilities::strchr(const char* str, char c) {
        while(true) {
            char v = *str;
            if(v == '\0')
                return nullptr;
            if(v == c)
                break;
            ++str;
        }
        
        return str;
    }

    // Definitions for FdtEngine

    const uint32_t* FdtEngine::get_aligned_after_offset(const uint32_t* ptr, std::size_t offset) { 
        return ptr + (offset/sizeof(uint32_t) + (offset % sizeof(uint32_t) ? 1 : 0));
    }

    const uint32_t* FdtEngine::move_to_next_token(const uint32_t* token_ptr) {
        uint32_t token = read_value(token_ptr);
        if(token == FDT_BEGIN_NODE) {
            ++token_ptr;
            const char* as_str = reinterpret_cast<const char*>(token_ptr);
            // If it is the root node, as it doesn't have a name, we just skip one token which is just padding.
            if(auto str_size = Utilities::strlen(as_str)) 
                token_ptr = get_aligned_after_offset(token_ptr, str_size + 1);
            else 
                ++token_ptr;
        } 
        else if(token == FDT_END_NODE) {
            ++token_ptr;
        }
        else if(token == FDT_PROP) {
            ++token_ptr;
            const fdt_prop_desc* descriptor = reinterpret_cast<const fdt_prop_desc*>(token_ptr);
            std::size_t prop_length = read_value(&descriptor->len);
            token_ptr = get_aligned_after_offset(token_ptr, sizeof(fdt_prop_desc) + prop_length);
        }
        else if(token == FDT_NOP) {
            ++token_ptr;
        }
        else if(token == FDT_END) {
            // Do nothing, there is nothing else to be read anymore
        }
        return token_ptr;
    }
    
    uint32_t FdtEngine::read_value(const uint32_t* ptr) {
        // Handles different endianess by reversing byte order if the target is little endian, given that values in the fdt header are all big endian
        if constexpr(__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__) {
            return ((*ptr & 0xFF) << 24) | ((*ptr & 0xFF00) << 8) | ((*ptr & 0xFF0000) >> 8) | ((*ptr & 0xFF000000) >> 24);
        }
        return *ptr;
    }

    const uint32_t* FdtEngine::get_structure_block_ptr(const fdt_header* header) {
        uint32_t structure_offset = read_value(&header->off_dt_struct);
        auto as_char_ptr = reinterpret_cast<const char*>(header) + structure_offset;
        return reinterpret_cast<const uint32_t*>(as_char_ptr);
    }

    const char* FdtEngine::get_string_block_ptr(const fdt_header* header) {
        uint32_t offset = read_value(&header->off_dt_strings);
        return (reinterpret_cast<const char*>(header)) + offset;
    }

    int FdtEngine::traverse_node(const uint32_t*& token_ptr, const fdt_header* header, TraversalAction& action) {
        const uint32_t* start_token = token_ptr;
        uint32_t token = read_value(token_ptr);
        
        // Given a node, it will traverse all of it subnodes recursively

        // The first token HAS to be a FDT_BEGIN_NODE, given that the function traverses a node to its end.
        if(token == FDT_BEGIN_NODE) {  
            
            action.on_FDT_BEGIN_NODE(header, token_ptr);
            token_ptr = move_to_next_token(token_ptr);

            while(true) {
                // If the action is satisfied, we have no reason at all to keep checking the remaing of the structure
                if(action.is_action_satisfied())
                    return ALL_OK;
                token = read_value(token_ptr);
                switch(token) {
                    case FDT_BEGIN_NODE:
                        // Special case, we have found another node!
                        {
                            auto retval = traverse_node(token_ptr, header, action);
                            if(retval != ALL_OK)
                                return retval;
                        }
                        break;
                    case FDT_END_NODE:
                        action.on_FDT_END_NODE(header, token_ptr);
                        token_ptr = move_to_next_token(token_ptr);
                        // If we started with the root node, the FDT_END token has to come next, so we check if this is the case
                        // in the next iteration of the loop.
                        if(start_token != get_structure_block_ptr(header))
                            return ALL_OK;
                        break;
                    case FDT_PROP:
                        action.on_FDT_PROP_NODE(header, token_ptr);
                        token_ptr = move_to_next_token(token_ptr);
                        break;
                    case FDT_NOP:
                        action.on_FDT_NOP_NODE(header, token_ptr);
                        token_ptr = move_to_next_token(token_ptr);
                        break;
                    case FDT_END:
                        // Finding a FDT_END token is only valid if we started with the root node, otherwise there is something wrong...
                        if(start_token == get_structure_block_ptr(header))
                            return ALL_OK;
                        [[fallthrough]];
                    default:
                        return INVALID_STRUCTURE_BLOCK;
                }
            }
        }

        // If we have found anything else, we don't have a valid structure block or something really weird happened
        return INVALID_STRUCTURE_BLOCK;
    }

    // Definitions for NodeFinder action

    NodeFinder::NodeFinder(const char* node_name, const char* unit_address) : m_node_name(node_name), m_unit_address(unit_address)  {
        // Empty
    };

    // Refactor, this is ugly af
    bool NodeFinder::is_same_as(const char* node_string) {
        if(node_string != nullptr && m_node_name != nullptr) {
            // If we care about unit address
            if(m_unit_address != nullptr) {
                auto at_loc = Utilities::strchr(node_string, '@');         
                if(at_loc != nullptr) {
                    size_t node_name_length = at_loc - node_string;
                    if(Utilities::strncmp(node_string, m_node_name, node_name_length) == 0) {
                        if(Utilities::strcmp(at_loc + 1, m_unit_address) == 0) {
                            return true;
                        }
                    }
                }
            }
            else {
                return Utilities::strcmp(node_string, m_node_name) == 0;
            }
        }

        return false;
    }

    void NodeFinder::on_FDT_BEGIN_NODE(const fdt_header* header, const uint32_t* token)  {
        if(m_node_name != nullptr) {
            const char* node_name = reinterpret_cast<const char*>(token + 1);
            if(is_same_as(node_name)) {
                m_result = FdtNode(token, header);
            }
        }   
    }

    bool NodeFinder::is_action_satisfied() const {
        return m_result.is_valid();
    }

    FdtNode NodeFinder::result() const {
        return m_result;    
    }

    // Definitions for PropertyFinder

    void PropertyFinder::on_FDT_PROP_NODE(const fdt_header* header, const uint32_t* token) {
        auto property_descriptor = reinterpret_cast<const fdt_prop_desc*>(token + 1);
        auto property_name = FdtEngine::get_string_block_ptr(header) + FdtEngine::read_value(&property_descriptor->nameoff);
        
        if(Utilities::strcmp(property_name, m_looked_property) == 0) {
            m_property_length = FdtEngine::read_value(&property_descriptor->len);
            if(m_property_length)
                m_result = token + sizeof(fdt_prop_desc)/sizeof(uint32_t) + 1; 
            m_property_found = true;
        }
    }

    bool PropertyFinder::is_action_satisfied() const {
        return is_property_found();
    }
    
    const void* PropertyFinder::property_content() const {
        return m_result;
    };
    
    uint32_t PropertyFinder::property_length() const {
        return m_property_length;
    }

    bool PropertyFinder::is_property_found() const {
        return m_property_found;
    }

    // Definitions for FdtNode 

    FdtNode::FdtNode(const uint32_t* first_token, const fdt_header* fdt_header) : m_node_token_start(first_token), m_fdt_header(fdt_header) {
        // Empty
    };

    FdtNode FdtNode::get_sub_node(const char* node_name, const char* unit_address) const {
        NodeFinder action(node_name, unit_address);
        const uint32_t* token_ptr = m_node_token_start;
        FdtEngine::traverse_node(token_ptr, m_fdt_header, action);
        return action.result();
    }

    const void* FdtNode::get_property(const char* property_name) const {
        PropertyFinder action(property_name);
        const uint32_t* token_ptr = m_node_token_start;
        FdtEngine::traverse_node(token_ptr, m_fdt_header, action);
        return action.property_content();
    }

    bool FdtNode::has_property(const char* property_name) const {
        PropertyFinder action(property_name);
        const uint32_t* token_ptr = m_node_token_start;
        FdtEngine::traverse_node(token_ptr, m_fdt_header, action);
        return action.is_property_found();
    }

    bool FdtNode::is_valid() const {
        return m_fdt_header != nullptr && m_node_token_start != nullptr;
    }

}

#endif
