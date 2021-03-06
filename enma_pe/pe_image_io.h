#pragma once

enum enma_io_mode {
    enma_io_mode_default,
    enma_io_mode_allow_expand,
};

enum enma_io_addressing_type {
    enma_io_address_raw,
    enma_io_address_rva,
};

enum enma_io_code {
    enma_io_success,
    enma_io_incomplete, //part of read\write
    enma_io_data_not_present,
};

inline bool view_data(//-> true if data or path of data is available or can be used trought adding size
    uint32_t  required_offset, uint32_t required_size, uint32_t& real_offset,
    uint32_t& available_size, uint32_t& down_oversize, uint32_t& up_oversize,
    uint32_t present_offset, uint32_t present_size);


#include "pe_section_io.h"

class pe_image_io{
    pe_image* image;
    uint32_t image_offset;

    enma_io_code last_code;
    enma_io_mode mode;
    enma_io_addressing_type addressing_type;

public:
    pe_image_io(
        pe_image& image,
        enma_io_mode mode = enma_io_mode_default,
        enma_io_addressing_type type = enma_io_address_rva
    );
    pe_image_io(
        const pe_image& image,
        enma_io_addressing_type type = enma_io_address_rva
    );

    pe_image_io(const pe_image_io& image_io);
    
    ~pe_image_io();

    pe_image_io& operator=(const pe_image_io& image_io);
public:
    template<typename T>
    pe_image_io& operator >> (const T& data) {//read from image

        read((void*)&data, sizeof(T));

        return *this;
    }
    template<typename T>
    pe_image_io& operator << (const T& data) {//write to image

        write((void*)&data, sizeof(T));

        return *this;
    }




    enma_io_code read(void * data, uint32_t size);
    enma_io_code write(const void * data, uint32_t size);

    enma_io_code read(std::vector<uint8_t>& buffer, uint32_t size);
    enma_io_code write(std::vector<uint8_t>& buffer);

    enma_io_code memory_set(uint32_t size,uint8_t data);

    enma_io_code read_string(std::string& _string);
    enma_io_code read_wstring(std::wstring& _wstring);

    enma_io_code internal_read(uint32_t data_offset,
        void * buffer, uint32_t size,
        uint32_t& readed_size, uint32_t& down_oversize, uint32_t& up_oversize
    );
    enma_io_code internal_write(uint32_t data_offset,
        const void * buffer, uint32_t size,
        uint32_t& wrote_size, uint32_t& down_oversize, uint32_t& up_oversize
    );

    bool view_image( //-> return like in view_data
        uint32_t required_offset, uint32_t required_size, uint32_t& real_offset,
        uint32_t& available_size, uint32_t& down_oversize, uint32_t& up_oversize);
public:
    pe_image_io&   set_mode(enma_io_mode mode);
    pe_image_io&   set_addressing_type(enma_io_addressing_type type);
    pe_image_io&   set_image_offset(uint32_t offset);

    pe_image_io& seek_to_start();
    pe_image_io& seek_to_end();
public:

    enma_io_mode            get_mode() const;
    enma_io_code            get_last_code() const;
    enma_io_addressing_type get_addressing_type() const;
    uint32_t                get_image_offset() const;

    bool                    is_executable_rva(uint32_t rva) const;
    bool                    is_writeable_rva(uint32_t rva) const;
    bool                    is_readable_rva(uint32_t rva) const;

    pe_image*               get_image();
};

inline bool view_data(
    uint32_t  required_offset, uint32_t required_size, uint32_t& real_offset,
    uint32_t& available_size, uint32_t& down_oversize, uint32_t& up_oversize,
    uint32_t present_offset, uint32_t present_size) {


    //         ...............
    //  .............................
    //  |    | |             |      |
    //  v    v |             |      |
    // (down_oversize)       |      |
    //         |             |      |
    //         v             v      |
    //         (available_size)     |
    //                       |      |
    //                       v      v
    //                       (up_oversize)

    real_offset = 0;
    available_size = 0;
    down_oversize = 0;
    up_oversize = 0;

    if (required_offset < present_offset) {
        down_oversize = (present_offset - required_offset);

        if (down_oversize >= required_size) {

            return false; //not in bounds
        }
        else {
            available_size = (required_size - down_oversize);

            if (available_size > present_size) {
                up_oversize = (available_size - present_size);
                available_size = present_size;

                return true;//partially in bounds
            }

            return true;//partially in bounds
        }
    }
    else {//if(required_offset >= present_offset)

        if (required_offset <= (present_offset + present_size)) {
            real_offset = (required_offset - present_offset);

            if (required_size + required_offset >(present_offset + present_size)) {
                up_oversize = (required_size + required_offset) - (present_offset + present_size);
                available_size = required_size - up_oversize;

                return true; //partially in bounds
            }
            else {
                available_size = required_size;

                return true; //full in bounds
            }
        }
        else {
            real_offset = (required_offset - present_offset + present_size);
            up_oversize = (required_size + required_offset) - (present_offset + present_size);
            available_size = 0;

            return true; //trough full adding size 
        }
    }

    return true;
}