#include "stdafx.h"
#include "pe_image_io.h"


pe_image_io::pe_image_io(
    pe_image& image,
    enma_io_mode mode,
    enma_io_addressing_type type
):image(&image), image_offset(0), last_code(enma_io_success), mode(mode), addressing_type(type){}


pe_image_io::pe_image_io(
    const pe_image& image,
    enma_io_addressing_type type
): image((pe_image*)&image), image_offset(0), last_code(enma_io_success), mode(enma_io_mode_default), addressing_type(type){}

pe_image_io::pe_image_io(const pe_image_io& image_io) {
    operator=(image_io);
}

pe_image_io::~pe_image_io() {

}

pe_image_io& pe_image_io::operator=(const pe_image_io& image_io) {
    this->image         = image_io.image;
    this->image_offset  = image_io.image_offset;
    this->last_code     = image_io.last_code;
    this->mode          = image_io.mode;
    this->addressing_type = image_io.addressing_type;

    return *this;
}





bool pe_image_io::view_image( //-> return like in view_data
    uint32_t required_offset, uint32_t required_size, uint32_t& real_offset,
    uint32_t& available_size, uint32_t& down_oversize, uint32_t& up_oversize) {


    if (image->get_sections_number()) {
        switch (addressing_type) {
        case enma_io_addressing_type::enma_io_address_raw: {
            pe_section * first_ = image->get_sections()[0];
            pe_section * last_ = image->get_last_section();


            return view_data(
                required_offset, required_size,
                real_offset, available_size, down_oversize, up_oversize,
                0, ALIGN_UP(
                    last_->get_pointer_to_raw() + last_->get_size_of_raw_data(),
                    image->get_file_align())
                + uint32_t(image->get_overlay_data().size()));
        }

        case enma_io_addressing_type::enma_io_address_rva: {
            pe_section * first_ = image->get_sections()[0];
            pe_section * last_ = image->get_last_section();

            return view_data(
                required_offset, required_size,
                real_offset, available_size, down_oversize, up_oversize,
                0, ALIGN_UP(
                    last_->get_virtual_address() + last_->get_virtual_size(),
                    image->get_section_align())
            );
        }

        default: {return false; }
        }
    }

    return false;
}


enma_io_code pe_image_io::internal_read(uint32_t data_offset,
    void * buffer, uint32_t size,
    uint32_t& readed_size, uint32_t& down_oversize, uint32_t& up_oversize
) {
    uint32_t real_offset = 0;

    bool b_view = view_image(data_offset, size,
        real_offset,
        readed_size, down_oversize, up_oversize);


    if (b_view && readed_size) {
        uint32_t total_readed_size    = 0;
        uint32_t total_up_oversize    = 0;

        uint32_t available_headers_size = (uint32_t)image->get_headers_data().size();
        uint32_t view_headers_size = addressing_type == enma_io_addressing_type::enma_io_address_raw ?
            ALIGN_UP(available_headers_size, image->get_file_align()) : ALIGN_UP(available_headers_size, image->get_section_align());


        if (data_offset < view_headers_size) {
            
            uint32_t header_readed_size = 0;
            uint32_t header_down_oversize = 0;
            uint32_t header_up_oversize = 0;

            
            b_view = view_data(
                data_offset, size,
                real_offset, header_readed_size, header_down_oversize, header_up_oversize,
                0, view_headers_size);

            if (b_view) {
                
                if (available_headers_size) {

                    if (available_headers_size >= (header_readed_size + real_offset) ) {
                        memcpy(&((uint8_t*)buffer)[header_down_oversize], &image->get_headers_data().data()[real_offset], header_readed_size);
                    }
                    else {
                        if (available_headers_size > real_offset) {
                            memcpy(&((uint8_t*)buffer)[header_down_oversize], &image->get_headers_data().data()[real_offset], 
                                (header_readed_size + real_offset) - available_headers_size
                            );

                            memset(&((uint8_t*)buffer)[header_down_oversize + (header_readed_size + real_offset) - available_headers_size], 0,
                                header_readed_size - ((header_readed_size + real_offset) - available_headers_size) );
                        }
                        else {
                            memset(&((uint8_t*)buffer)[header_down_oversize], 0, header_readed_size);
                        }                  
                    }
                }
                else {
                    memset(&((uint8_t*)buffer)[header_down_oversize], 0, header_readed_size);
                }

                total_readed_size += header_readed_size;
                total_up_oversize = header_up_oversize;
            }
        }

        for (auto &section : image->get_sections()) {

            if (total_readed_size == readed_size) { break; }

            uint32_t sec_readed_size   = 0;
            uint32_t sec_down_oversize = 0;
            uint32_t sec_up_oversize   = 0;

            pe_section_io(*section, *image, mode, addressing_type).internal_read(
                data_offset, buffer, size, sec_readed_size, sec_down_oversize, sec_up_oversize
            );

            total_readed_size += sec_readed_size;
            total_up_oversize = sec_up_oversize;

            if (!total_up_oversize) { break; }
        }

        if (addressing_type == enma_io_addressing_type::enma_io_address_raw &&
            total_up_oversize && image->get_overlay_data().size()) { //take up size from overlay

            uint32_t top_section_raw = 0;

            uint32_t overlay_readed_size = 0;
            uint32_t overlay_down_oversize = 0;
            uint32_t overlay_up_oversize = 0;

            if (image->get_sections_number()) {
                top_section_raw = 
                    ALIGN_UP(image->get_last_section()->get_pointer_to_raw() + image->get_last_section()->get_size_of_raw_data(), image->get_file_align());
            }

            b_view = view_data(
                data_offset, size,
                real_offset, overlay_readed_size, overlay_down_oversize, overlay_up_oversize,
                top_section_raw, uint32_t(image->get_overlay_data().size()));
            
            if (b_view) {
                memcpy(&((uint8_t*)buffer)[down_oversize], &image->get_overlay_data().data()[real_offset],overlay_readed_size);


                total_readed_size += overlay_readed_size;
                total_up_oversize = overlay_up_oversize;
            }
        }

        readed_size = total_readed_size;
        up_oversize = total_up_oversize;

        if (down_oversize || up_oversize) {
            last_code = enma_io_code::enma_io_incomplete;
            return enma_io_code::enma_io_incomplete;
        }

        last_code = enma_io_code::enma_io_success;
        return enma_io_code::enma_io_success;
    }

    last_code = enma_io_code::enma_io_data_not_present;
    return enma_io_code::enma_io_data_not_present;
}

enma_io_code pe_image_io::internal_write(uint32_t data_offset,
    const void * buffer, uint32_t size,
    uint32_t& wrote_size, uint32_t& down_oversize, uint32_t& up_oversize
) {

    uint32_t real_offset = 0;

    bool b_view = view_image(data_offset, size,
        real_offset,
        wrote_size, down_oversize, up_oversize);


    if (b_view &&
        (wrote_size || (up_oversize && mode == enma_io_mode::enma_io_mode_allow_expand))) {

        uint32_t total_wroted_size   = 0;
        uint32_t total_up_oversize   = 0;

        for (size_t section_idx = 0; section_idx < image->get_sections().size(); section_idx++) {

            uint32_t sec_wroted_size   = 0;
            uint32_t sec_down_oversize = 0;
            uint32_t sec_up_oversize   = 0;

            pe_section_io section_io(*image->get_sections()[section_idx], *image,
                (section_idx == (image->get_sections().size()-1) &&
                    mode == enma_io_mode::enma_io_mode_allow_expand) ? enma_io_mode_allow_expand : enma_io_mode_default,addressing_type);


            section_io.internal_write(data_offset, buffer, size,
                sec_wroted_size, sec_down_oversize, sec_up_oversize);


            total_wroted_size += sec_wroted_size;
            total_up_oversize  = sec_up_oversize;

            if (!total_up_oversize) { break; }
        }
        up_oversize = total_up_oversize;
        wrote_size  = total_wroted_size;

        if (down_oversize || up_oversize) {

            last_code = enma_io_code::enma_io_incomplete;
            return enma_io_code::enma_io_incomplete;
        }

        last_code = enma_io_code::enma_io_success;
        return enma_io_code::enma_io_success;
    }

    last_code = enma_io_code::enma_io_data_not_present;
    return enma_io_code::enma_io_data_not_present;
}

enma_io_code pe_image_io::read(void * data, uint32_t size) {

    uint32_t readed_size;
    uint32_t down_oversize;
    uint32_t up_oversize;


    enma_io_code code = internal_read(image_offset, data, size,
        readed_size, down_oversize, up_oversize);

    image_offset += readed_size;

    return code;
}

enma_io_code pe_image_io::write(const void * data, uint32_t size) {

    uint32_t wrote_size;
    uint32_t down_oversize;
    uint32_t up_oversize;

    enma_io_code code = internal_write(image_offset, data, size,
        wrote_size, down_oversize, up_oversize);

    image_offset += wrote_size;

    return code;
}

enma_io_code pe_image_io::read(std::vector<uint8_t>& buffer, uint32_t size) {

    if (buffer.size() < size) { buffer.resize(size); }

    return read(buffer.data(), uint32_t(buffer.size()));
}

enma_io_code pe_image_io::write(std::vector<uint8_t>& buffer) {

    return write(buffer.data(), uint32_t(buffer.size()));
}

enma_io_code pe_image_io::memory_set(uint32_t size, uint8_t data) {

    std::vector<uint8_t> set_buffer;
    set_buffer.resize(size);
    memset(set_buffer.data(), data, size);

    return write(set_buffer);
}

enma_io_code pe_image_io::read_string(std::string& _string) {

    _string.clear();
    char _char = 0;

    do {
        enma_io_code code = read(&_char, sizeof(_char));

        if (code != enma_io_success) {
            return code;
        }

        if (_char) {
            _string += _char;
        }

    } while (_char);

    return enma_io_code::enma_io_success;
}

enma_io_code pe_image_io::read_wstring(std::wstring& _wstring) {

    _wstring.clear();
    wchar_t _char = 0;

    do {
        enma_io_code code = read(&_char, sizeof(_char));

        if (code != enma_io_success) {
            return code;
        }

        if (_char) {
            _wstring += _char;
        }

    } while (_char);

    return enma_io_code::enma_io_success;
}


pe_image_io& pe_image_io::set_mode(enma_io_mode mode) {

    this->mode = mode;

    return *this;
}
pe_image_io& pe_image_io::set_addressing_type(enma_io_addressing_type type) {

    this->addressing_type = type;

    return *this;
}
pe_image_io& pe_image_io::set_image_offset(uint32_t offset) {

    this->image_offset = offset;

    return *this;
}

pe_image_io& pe_image_io::seek_to_start() {

    this->image_offset = 0;

    return *this;
}

pe_image_io& pe_image_io::seek_to_end() {

    if (image->get_sections_number()) {
        switch (addressing_type) {
        case enma_io_addressing_type::enma_io_address_raw: {
            this->image_offset = ALIGN_UP(
                image->get_last_section()->get_pointer_to_raw() + image->get_last_section()->get_size_of_raw_data(),
                image->get_file_align()
            );
            break;
        }

        case enma_io_addressing_type::enma_io_address_rva: {
            this->image_offset = ALIGN_UP(
                image->get_last_section()->get_virtual_address() + image->get_last_section()->get_virtual_size(),
                image->get_section_align()
            );
            break;
        }

        default: {this->image_offset = 0; break; }
        }
    }
    else {
        this->image_offset = 0;
    }

    return *this;
}

enma_io_mode  pe_image_io::get_mode() const {
    return this->mode;
}
enma_io_code  pe_image_io::get_last_code() const {
    return this->last_code;
}
enma_io_addressing_type pe_image_io::get_addressing_type() const {
    return this->addressing_type;
}
uint32_t  pe_image_io::get_image_offset() const {
    return this->image_offset;
}

bool  pe_image_io::is_executable_rva(uint32_t rva) const {

    pe_section * rva_section = image->get_section_by_rva(rva);

    if (rva_section) {
        return rva_section->is_executable();
    }

    return false;
}
bool  pe_image_io::is_writeable_rva(uint32_t rva) const {
    pe_section * rva_section = image->get_section_by_rva(rva);

    if (rva_section) {
        return rva_section->is_writeable();
    }

    return false;
}
bool  pe_image_io::is_readable_rva(uint32_t rva) const {
    pe_section * rva_section = image->get_section_by_rva(rva);

    if (rva_section) {
        return rva_section->is_readable();
    }

    return false;
}

pe_image*  pe_image_io::get_image() {
    return this->image;
}



