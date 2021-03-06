#pragma once

class exceptions_item {
    uint32_t address_begin;
    uint32_t address_end;
    uint32_t address_unwind_data;
public:
    exceptions_item();
    exceptions_item(const exceptions_item& item);
    exceptions_item(uint32_t address_begin, uint32_t address_end, uint32_t address_unwind_data);
    ~exceptions_item();

    exceptions_item& operator=(const exceptions_item& item);
public:
    void set_begin_address(uint32_t rva_address);
    void set_end_address(uint32_t rva_address);
    void set_unwind_data_address(uint32_t rva_address);


public:
    uint32_t get_begin_address() const;
    uint32_t get_end_address() const;
    uint32_t get_unwind_data_address() const;

};

class exceptions_table {

    std::vector<exceptions_item> items;
public:
    exceptions_table();
    exceptions_table(const exceptions_table& exceptions);
    ~exceptions_table();

    exceptions_table& operator=(const exceptions_table& exceptions);
public:
    void add_item(uint32_t address_begin, uint32_t address_end, uint32_t address_unwind_data);
    void add_item(const exceptions_item& item);
    void add_item(const image_ia64_runtime_function_entry& exc_entry);
    void clear();
public:
    size_t size() const;

    const std::vector<exceptions_item>& get_items() const;
    std::vector<exceptions_item>& get_items();
};



directory_code get_exception_table(_In_ const pe_image &image, _Out_ exceptions_table& exceptions);
bool build_exceptions_table(_Inout_ pe_image &image, _Inout_ pe_section& section,
    _In_ const exceptions_table& exceptions);
directory_code get_placement_exceptions_table(_In_ const pe_image &image, _Inout_ pe_directory_placement& placement);