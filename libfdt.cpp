#include "libfdt.hpp"


namespace fdt {

    size_t Utilities::strlen(const char* str) {
        size_t i = 0;
        for(;str[i] != '\0'; ++i);
        return i;
    }

    // Definitions for FdtEngine

    const uint32_t* FdtEngine::get_aligned_after_offset(const uint32_t* ptr, std::size_t offset) { 
        return ptr + (offset/sizeof(uint32_t) + (offset % sizeof(uint32_t) ? 1 : 0));
    }

    const uint32_t* FdtEngine::get_next_token(const uint32_t* token_ptr) {
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


    // The recursive call stack for the function is equal to the depth of the tree. Unless we are on a really constrained environment,
    // this shouldn't be a big deal.
    int FdtEngine::traverse_node(const uint32_t*& token_ptr, const fdt_header* header, TraversalAction& action) {
        const uint32_t* start_token = token_ptr;
        uint32_t token = read_value(token_ptr);
        
        // Given a node, it will traverse all of it subnodes recursively

        // The first token HAS to be a FDT_BEGIN_NODE, given that the function traverses a node to its end.
        if(token == FDT_BEGIN_NODE) {  
            
            action.on_FDT_BEGIN_NODE(header, token_ptr);
            token_ptr = get_next_token(token_ptr);

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
                        token_ptr = get_next_token(token_ptr);
                        // If we started with the root node, the FDT_END token has to come next, so we check if this is the case
                        // in the next iteration of the loop.
                        if(start_token != get_structure_block_ptr(header))
                            return ALL_OK;
                        break;
                    case FDT_PROP:
                        action.on_FDT_PROP_NODE(header, token_ptr);
                        token_ptr = get_next_token(token_ptr);
                        break;
                    case FDT_NOP:
                        action.on_FDT_NOP_NODE(header, token_ptr);
                        token_ptr = get_next_token(token_ptr);
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

}
