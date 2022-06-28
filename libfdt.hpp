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
        static size_t strlen(const char* str);
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
        static const uint32_t* get_next_token(const uint32_t* token_ptr);

        public:
    
        static uint32_t read_value(const uint32_t* ptr);
        static const uint32_t* get_structure_block_ptr(const fdt_header* header);
        static const char* get_string_block_ptr(const fdt_header* header);
        static int traverse_node(const uint32_t*& token_ptr, const fdt_header* header, TraversalAction& action);
        static int traverse_fdt(const fdt_header* header, TraversalAction& action);
 
    };
    
}    

#endif
