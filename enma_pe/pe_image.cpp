#include "stdafx.h"
#include "pe_image.h"


pe_image::pe_image() {
	clear_image();
}
pe_image::pe_image(const pe_image& image) {
    this->operator=(image);
}

pe_image::pe_image(bool _pe32) {
	clear_image();

	if (_pe32) {
		magic = IMAGE_NT_OPTIONAL_HDR32_MAGIC;
	}
	else {
		magic = IMAGE_NT_OPTIONAL_HDR64_MAGIC;
	}

	image_status = pe_image_status_ok;
}

pe_image::pe_image(const uint8_t* pe_image, uint32_t size) {
	clear_image();
	init_from_file(pe_image, size);
}

pe_image::pe_image(const std::string& file_path) {
	clear_image();

    FILE* hfile = fopen(file_path.c_str(), "rb");


    if (hfile != nullptr) {
        fseek(hfile, 0, SEEK_END);
        size_t file_size = ftell(hfile);
        fseek(hfile, 0, SEEK_SET);

        uint8_t * file_buffer = new uint8_t[file_size];


        if (fread(file_buffer, file_size, 1, hfile)) {

            init_from_file(file_buffer, file_size);
        }
        else {
            image_status = pe_image_status_bad_format;
        }

        delete[] file_buffer;

        fclose(hfile);
    }
	else {
		image_status = pe_image_status_unknown;
	}
}

pe_image::~pe_image() {
	clear_image();
}

pe_image& pe_image::operator=(const pe_image& image) {
	clear_image();

    headers_data = image.headers_data;

	image_status        = image.image_status;
	machine	            = image.machine;
	timestamp           = image.timestamp;
	characteristics     = image.characteristics;
	magic               = image.magic;
	major_linker        = image.major_linker;
	minor_linker        = image.minor_linker;
	size_of_code        = image.size_of_code;
	size_of_init_data   = image.size_of_init_data;
	size_of_uninit_data = image.size_of_uninit_data;
	entry_point         = image.entry_point;
	base_of_code        = image.base_of_code;
	base_of_data        = image.base_of_data;
	image_base          = image.image_base;
	section_align       = image.section_align;
	file_align          = image.file_align;
	os_ver_major        = image.os_ver_major;
	os_ver_minor        = image.os_ver_minor;
	image_ver_major     = image.image_ver_major;
	image_ver_minor     = image.image_ver_minor;
	subsystem_ver_major = image.subsystem_ver_major;
	subsystem_ver_minor = image.subsystem_ver_minor;
	image_size          = image.image_size;
	headers_size        = image.headers_size;
	checksum            = image.checksum;
	sub_system          = image.sub_system;
	characteristics_dll = image.characteristics_dll;
	stack_reserve_size  = image.stack_reserve_size;
	stack_commit_size   = image.stack_commit_size;
	heap_reserve_size   = image.heap_reserve_size;
	heap_commit_size    = image.heap_commit_size;

    for (size_t i = 0; i < 16; i++) {
        directories[i].virtual_address = image.directories[i].virtual_address;
        directories[i].size           = image.directories[i].size;
    }

	for (size_t i = 0; i < image.get_sections_number(); i++) {
		this->add_section(*image.sections[i]);
	}


    overlay_data = image.overlay_data;

	return *this;
}

template <typename image_format>
bool init_nt_header(pe_image& image,void * nt_header,uint32_t& sections_offset,uint32_t& number_of_sections) {

    typename image_format::image_nt_headers * header = (typename image_format::image_nt_headers *)nt_header;

    if (header->optional_header.magic == image_format::image_magic) {

        image.set_machine(header->file_header.machine);
        image.set_timestamp(header->file_header.time_date_stamp);
        image.set_characteristics(header->file_header.characteristics);
        image.set_magic(header->optional_header.magic);
        image.set_major_linker(header->optional_header.major_linker_version);
        image.set_minor_linker(header->optional_header.minor_linker_version);
        image.set_size_of_code(header->optional_header.size_of_code);
        image.set_size_of_init_data(header->optional_header.size_of_initialized_data);
        image.set_size_of_uninit_data(header->optional_header.size_of_uninitialized_data);
        image.set_entry_point(header->optional_header.address_of_entry_point);
        image.set_base_of_code(header->optional_header.base_of_code);
        image.set_image_base(header->optional_header.image_base);
        image.set_section_align(header->optional_header.section_alignment);
        image.set_file_align(header->optional_header.file_alignment);
        image.set_os_ver_major(header->optional_header.major_operating_system_version);
        image.set_os_ver_minor(header->optional_header.minor_operating_system_version);
        image.set_image_ver_major(header->optional_header.major_image_version);
        image.set_image_ver_minor(header->optional_header.minor_image_version);
        image.set_subsystem_ver_major(header->optional_header.major_subsystem_version);
        image.set_subsystem_ver_minor(header->optional_header.minor_subsystem_version);
        image.set_image_size(header->optional_header.size_of_image);
        image.set_headers_size(header->optional_header.size_of_headers);
        image.set_checksum(header->optional_header.checksum);
        image.set_sub_system(header->optional_header.subsystem);
        image.set_characteristics_dll(header->optional_header.dll_characteristics);
        image.set_stack_reserve_size(header->optional_header.size_of_stack_reserve);
        image.set_stack_commit_size(header->optional_header.size_of_stack_commit);
        image.set_heap_reserve_size(header->optional_header.size_of_heap_reserve);
        image.set_heap_commit_size(header->optional_header.size_of_heap_commit);

        for (uint32_t i = 0; i < 16; i++) {
            image.set_directory_virtual_address(i, header->optional_header.data_directory[i].virtual_address);
            image.set_directory_virtual_size(i, header->optional_header.data_directory[i].size);
        }

        sections_offset   += header->file_header.size_of_optional_header;
        number_of_sections = header->file_header.number_of_sections;
        return true;
    }

    return false;
}


void pe_image::init_from_file(const uint8_t * image, size_t size) {
	if (size < sizeof(image_dos_header)) {this->image_status = pe_image_status_bad_format;return;};

    image_dos_header dos_header;

    memcpy(&dos_header, image, sizeof(image_dos_header));

	if (dos_header.e_magic == IMAGE_DOS_SIGNATURE) { //check MZ sign

		if (*(uint32_t*)(&image[dos_header.e_lfanew]) == IMAGE_NT_SIGNATURE) { //check PE00 sign
			uint32_t section_offset = dos_header.e_lfanew + INT32_SIZE + (uint32_t)sizeof(image_file_header);
			uint32_t number_of_sections = 0;
			
            if (size < section_offset + sizeof(image_nt_headers32)) { this->image_status = pe_image_status_bad_format; return; };

            if (init_nt_header<image_32>(*this, (void*)&image[dos_header.e_lfanew], section_offset, number_of_sections)) {
                set_base_of_data(pimage_nt_headers32(&image[dos_header.e_lfanew])->optional_header.base_of_data);
            }
            else {
                if (size < section_offset + sizeof(image_nt_headers64)) { this->image_status = pe_image_status_bad_format; return; };

                if (!init_nt_header<image_64>(*this, (void*)&image[dos_header.e_lfanew], section_offset, number_of_sections)) {
                    this->image_status = pe_image_status_bad_format;
                    return;
                }

                set_base_of_data(0);
            }

            headers_data.resize(this->headers_size);
            memcpy(headers_data.data(), image, this->headers_size);

            {
                uint32_t image_top_size = 0;
                for (size_t i = 0; i < number_of_sections; i++) {
                    image_section_header* section_image = (image_section_header*)(&image[section_offset]);

                    if (size < section_offset + sizeof(image_section_header) ||
                        size < section_image->pointer_to_raw_data + section_image->size_of_raw_data
                        ) {

                        this->image_status = pe_image_status_bad_format; return;
                    };

                    std::vector<uint8_t> section_data;
                    section_data.resize(section_image->size_of_raw_data);
                    memcpy(section_data.data(), &image[section_image->pointer_to_raw_data], section_image->size_of_raw_data);

                    add_section(pe_section(*section_image, section_data));
                    section_offset += (uint16_t)sizeof(image_section_header);

                    image_top_size = ALIGN_UP((section_image->pointer_to_raw_data + section_image->size_of_raw_data), this->file_align);
                }

                

                if (image_top_size < size) {
                    overlay_data.resize(size - image_top_size);
                    memcpy(overlay_data.data(), &image[image_top_size], size - image_top_size);
                }
            }

			this->image_status = pe_image_status_ok;
			return;
		}

	}

	this->image_status = pe_image_status_bad_format;
}


bool	pe_image::is_x32_image() const{
	if (this->magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
		return true;
	}
	return false;
}
bool	pe_image::is_dll_image()  const {
	if (this->characteristics&IMAGE_FILE_DLL) {
		return true;
	}
	return false;
}
bool	pe_image::is_dotnet_image() const {
	return get_directory_virtual_address(IMAGE_DIRECTORY_ENTRY_COM_DESCRIPTOR) != 0;
}

void pe_image::clear_image() {
	image_status = pe_image_status_unknown;
	machine				= 0;
	timestamp			= 0;
	characteristics		= 0;
	magic				= 0;
	major_linker		= 0;
	minor_linker		= 0;
	size_of_code		= 0;
	size_of_init_data	= 0;
	size_of_uninit_data = 0;
	entry_point			= 0;
	base_of_code		= 0;
	base_of_data		= 0;
	image_base			= 0;
	section_align		= 0;
	file_align			= 0;
	os_ver_major		= 0;
	os_ver_minor		= 0;
	image_ver_major		= 0;
	image_ver_minor		= 0;
	subsystem_ver_major = 0;
	subsystem_ver_minor = 0;
	image_size			= 0;
	headers_size		= 0;
	checksum			= 0;
	sub_system			= 0;
	characteristics_dll = 0;
	stack_reserve_size	= 0;
	stack_commit_size	= 0;
	heap_reserve_size	= 0;
	heap_commit_size	= 0;


    for (size_t i = 0; i < 16; i++) {
        directories[i].virtual_address = 0;
        directories[i].size            = 0;
    }

	while (sections.size()) { delete sections[sections.size() - 1]; sections.pop_back(); }

    headers_data.clear();
}




pe_section& pe_image::add_section() {

	uint32_t virtual_address = section_align;
	uint32_t raw_pointer = file_align;

	if (sections.size()) {
		virtual_address = ALIGN_UP(sections[sections.size() - 1]->get_virtual_address() +
			sections[sections.size() - 1]->get_virtual_size(), section_align);

		raw_pointer = ALIGN_UP(sections[sections.size() - 1]->get_pointer_to_raw() +
			sections[sections.size() - 1]->get_size_of_raw_data(), file_align);

		if (!sections[sections.size() - 1]->get_virtual_size())		{ virtual_address += section_align; }
	}

	sections.push_back(new pe_section());
	pe_section& section =  *sections[sections.size() - 1];

	section.set_virtual_address(virtual_address);
	section.set_pointer_to_raw(raw_pointer);

	return section;
}
pe_section& pe_image::add_section(const pe_section& section) {
	return (add_section() = section);
}
pe_section& pe_image::add_section(const image_section_header& header) {
	sections.push_back(new pe_section(header));
	return *sections[sections.size() - 1];
}
pe_section& pe_image::add_section(const image_section_header& header, const std::vector<uint8_t>& data) {
	sections.push_back(new pe_section(header, data));
	return *sections[sections.size() - 1];
}

std::vector<pe_section*>& pe_image::get_sections() {
	return this->sections;
}

size_t pe_image::get_sections_number() const {
	return this->sections.size();
}

pe_section*	 pe_image::get_section_by_rva(uint32_t rva) const {
	for (size_t section_idx = 0; section_idx < sections.size(); section_idx++) {
		if (
			sections[section_idx]->get_virtual_address() <= rva &&
			sections[section_idx]->get_virtual_address() + sections[section_idx]->get_virtual_size() > rva
			) {
			return sections[section_idx];
		}
	}
	return 0;
}
pe_section*	 pe_image::get_section_by_va(uint64_t va) const {
	return get_section_by_rva(va_to_rva(va));
}
pe_section*	 pe_image::get_section_by_raw(uint32_t raw) const {
	for (size_t section_idx = 0; section_idx < sections.size(); section_idx++) {
		if (
			sections[section_idx]->get_pointer_to_raw() <= raw &&
			sections[section_idx]->get_pointer_to_raw() + sections[section_idx]->get_size_of_raw_data() > raw
			) {
			return sections[section_idx];
		}
	}
	return 0;
}
pe_section*	 pe_image::get_section_by_idx(uint32_t idx) const {
	if (this->sections.size() > idx) {
		return this->sections[idx];
	}

	return 0;
}
pe_section*	 pe_image::get_last_section() const {
    if (this->sections.size()) {
        return this->sections[this->sections.size() - 1];
    }
    
    return 0;
}

uint32_t    pe_image::va_to_rva(uint64_t va) const {
    if (va) {
        return uint32_t(va - this->image_base);
    }
    else {
        return 0;
    }
}
uint32_t    pe_image::va_to_raw(uint64_t va) const {

	pe_section * section = get_section_by_va(va);
	if (section)
		return (va_to_rva(va) - section->get_virtual_address()) + section->get_pointer_to_raw();
	else
		return 0;
}
uint64_t    pe_image::rva_to_va(uint32_t rva) const {
	return (rva + this->image_base);
}
uint32_t    pe_image::rva_to_raw(uint32_t rva) const {

	pe_section * section = get_section_by_rva(rva);
	if (section)
		return (rva - section->get_virtual_address()) + section->get_pointer_to_raw();
	else
		return 0;

}
uint64_t    pe_image::raw_to_va(uint32_t raw) const {
	return rva_to_va(raw_to_rva(raw));
}
uint32_t    pe_image::raw_to_rva(uint32_t raw) const {
	pe_section * section = get_section_by_raw(raw);
	if (section)
		return  (raw - section->get_pointer_to_raw()) + section->get_virtual_address();
	else
		return 0;
}


void    pe_image::set_image_status(pe_image_status status) {
	this->image_status = status;
}
void    pe_image::set_headers_data(const std::vector<uint8_t>& headers_data) {
    this->headers_data = headers_data;
}
void    pe_image::set_dos_header(const image_dos_header& header) {

    if (headers_data.size() < sizeof(image_dos_header)) {
        headers_data.resize(sizeof(image_dos_header));
    }

    memcpy(headers_data.data(), &header, sizeof(image_dos_header));
}
void    pe_image::set_machine(uint16_t machine) {
	this->machine = machine;
}
void    pe_image::set_timestamp(uint32_t timestamp) {
	this->timestamp = timestamp;
}
void    pe_image::set_characteristics(uint16_t characteristics) {
	this->characteristics = characteristics;
}
void    pe_image::set_magic(uint16_t magic) {
	this->magic = magic;
}
void    pe_image::set_major_linker(uint8_t major_linker) {
	this->major_linker = major_linker;
}
void    pe_image::set_minor_linker(uint8_t minor_linker){
	this->minor_linker = minor_linker;
}
void    pe_image::set_size_of_code(uint32_t size_of_code) {
	this->size_of_code = size_of_code;
}
void    pe_image::set_size_of_init_data(uint32_t size_of_init_data) {
	this->size_of_init_data = size_of_init_data;
}
void    pe_image::set_size_of_uninit_data(uint32_t size_of_uninit_data) {
	this->size_of_uninit_data = size_of_uninit_data;
}
void    pe_image::set_entry_point(uint32_t	entry_point) {
	this->entry_point = entry_point;
}
void    pe_image::set_base_of_code(uint32_t base_of_code) {
	this->base_of_code = base_of_code;
}
void    pe_image::set_base_of_data(uint32_t base_of_data) {
	this->base_of_data = base_of_data;
}
void    pe_image::set_image_base(uint64_t image_base) {
	this->image_base = image_base;
}
void    pe_image::set_section_align(uint32_t section_align) {
	this->section_align = section_align;
}
void    pe_image::set_file_align(uint32_t file_align) {
	this->file_align = file_align;
}
void    pe_image::set_os_ver_major(uint16_t os_ver_major) {
	this->os_ver_major = os_ver_major;
}
void    pe_image::set_os_ver_minor(uint16_t os_ver_minor) {
	this->os_ver_minor = os_ver_minor;
}
void    pe_image::set_image_ver_major(uint16_t image_ver_major) {
	this->image_ver_major = image_ver_major;
}
void    pe_image::set_image_ver_minor(uint16_t image_ver_minor) {
	this->image_ver_minor = image_ver_minor;
}
void    pe_image::set_subsystem_ver_major(uint16_t subsystem_ver_major) {
	this->subsystem_ver_major = subsystem_ver_major;
}
void    pe_image::set_subsystem_ver_minor(uint16_t subsystem_ver_minor) {
	this->subsystem_ver_minor = subsystem_ver_minor;
}
void    pe_image::set_image_size(uint32_t image_size) {
	this->image_size = image_size;
}
void    pe_image::set_headers_size(uint32_t headers_size) {
	this->headers_size = headers_size;
}
void    pe_image::set_checksum(uint32_t checksum) {
	this->checksum = checksum;
}
void    pe_image::set_sub_system(uint16_t sub_system) {
	this->sub_system = sub_system;
}
void    pe_image::set_characteristics_dll(uint16_t characteristics_dll) {
	this->characteristics_dll = characteristics_dll;
}
void    pe_image::set_stack_reserve_size(uint64_t stack_reserve_size) {
	this->stack_reserve_size = stack_reserve_size;
}
void    pe_image::set_stack_commit_size(uint64_t stack_commit_size) {
	this->stack_commit_size = stack_commit_size;
}
void    pe_image::set_heap_reserve_size(uint64_t heap_reserve_size) {
	this->heap_reserve_size = heap_reserve_size;
}
void    pe_image::set_heap_commit_size(uint64_t heap_commit_size) {
	this->heap_commit_size = heap_commit_size;
}
void    pe_image::set_overlay_data(std::vector<uint8_t>& data) {
    this->overlay_data = data;
}
void    pe_image::set_directory_virtual_address(uint32_t directory_idx, uint32_t virtual_address) {
	if (directory_idx < IMAGE_NUMBEROF_DIRECTORY_ENTRIES) {
		this->directories[directory_idx].virtual_address = virtual_address;
	}
}
void    pe_image::set_directory_virtual_size(uint32_t directory_idx, uint32_t virtual_size) {
	if (directory_idx < IMAGE_NUMBEROF_DIRECTORY_ENTRIES) {
		this->directories[directory_idx].size = virtual_size;
	}
}


pe_image_status pe_image::get_image_status() const {
	return image_status;
}
const std::vector<uint8_t>& pe_image::get_headers_data() const {
    return headers_data;
}
uint16_t    pe_image::get_machine() const {
	return machine;
}
uint32_t    pe_image::get_timestamp() const {
	return timestamp;
}
uint16_t    pe_image::get_characteristics() const {
	return characteristics;
}
uint16_t    pe_image::get_magic() const {
	return magic;
}
uint8_t     pe_image::get_major_linker() const {
	return major_linker;
}
uint8_t     pe_image::get_minor_linker() const {
	return minor_linker;
}
uint32_t    pe_image::get_size_of_code() const {
	return size_of_code;
}
uint32_t    pe_image::get_size_of_init_data() const {
	return size_of_init_data;
}
uint32_t    pe_image::get_size_of_uninit_data() const {
	return size_of_uninit_data;
}
uint32_t    pe_image::get_entry_point() const {
	return entry_point;
}
uint32_t    pe_image::get_base_of_code() const {
	return base_of_code;
}
uint32_t    pe_image::get_base_of_data() const {
	return base_of_data;
}
uint64_t   pe_image::get_image_base() const {
	return image_base;
}
uint32_t    pe_image::get_section_align() const {
	return section_align;
}
uint32_t    pe_image::get_file_align() const {
	return file_align;
}
uint16_t    pe_image::get_os_ver_major() const {
	return os_ver_major;
}
uint16_t    pe_image::get_os_ver_minor() const {
	return os_ver_minor;
}
uint16_t    pe_image::get_image_ver_major() const {
	return image_ver_major;
}
uint16_t    pe_image::get_image_ver_minor() const {
	return image_ver_minor;
}
uint16_t    pe_image::get_subsystem_ver_major() const {
	return subsystem_ver_major;
}
uint16_t    pe_image::get_subsystem_ver_minor() const {
	return subsystem_ver_minor;
}
uint32_t    pe_image::get_image_size() const {
	return image_size;
}
uint32_t    pe_image::get_headers_size() const {
	return headers_size;
}
uint32_t    pe_image::get_checksum() const {
	return checksum;
}
uint16_t    pe_image::get_sub_system() const {
	return sub_system;
}
uint16_t    pe_image::get_characteristics_dll() const {
	return characteristics_dll;
}
uint64_t   pe_image::get_stack_reserve_size() const {
	return stack_reserve_size;
}
uint64_t   pe_image::get_stack_commit_size() const {
	return stack_commit_size;
}
uint64_t   pe_image::get_heap_reserve_size() const {
	return heap_reserve_size;
}
uint64_t   pe_image::get_heap_commit_size() const {
	return heap_commit_size;
}

std::vector<uint8_t>&   pe_image::get_overlay_data() {
    return overlay_data;
}

uint32_t    pe_image::get_directory_virtual_address(uint32_t directory_idx) const {
	if (directory_idx < IMAGE_NUMBEROF_DIRECTORY_ENTRIES) {
		return this->directories[directory_idx].virtual_address;
	}

	return 0;
}
uint32_t    pe_image::get_directory_virtual_size(uint32_t directory_idx) const {
	if (directory_idx < IMAGE_NUMBEROF_DIRECTORY_ENTRIES) {
		return this->directories[directory_idx].size;
	}

	return 0;
}

bool        pe_image::has_directory(uint32_t directory_idx) const {
	if (directory_idx < IMAGE_NUMBEROF_DIRECTORY_ENTRIES) {
		return (this->directories[directory_idx].virtual_address != 0);
	}
	return false;
}

void  pe_image::get_dos_header(image_dos_header &header) const {

    memset(&header, 0, sizeof(image_dos_header));

    if (headers_data.size() < sizeof(image_dos_header)) {
        memcpy(&header, headers_data.data(), headers_data.size());
    }
    else {
        memcpy(&header, headers_data.data(), sizeof(image_dos_header));
    }
}