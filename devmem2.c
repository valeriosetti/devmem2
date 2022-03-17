/*
 * devmem2.c: Simple program to read/write from/to any location in memory.
 *
 *  Copyright (C) 2000, Jan-Derk Bakker (J.D.Bakker@its.tudelft.nl)
 *
 *
 * This software has been developed for the LART computing board
 * (http://www.lart.tudelft.nl/). The development has been sponsored by
 * the Mobile MultiMedia Communications (http://www.mmc.tudelft.nl/)
 * and Ubiquitous Communications (http://www.ubicom.tudelft.nl/)
 * projects.
 *
 * The author can be reached at:
 *
 *  Jan-Derk Bakker
 *  Information and Communication Theory Group
 *  Faculty of Information Technology and Systems
 *  Delft University of Technology
 *  P.O. Box 5031
 *  2600 GA Delft
 *  The Netherlands
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <ctype.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/mman.h>
  
#define FATAL \
	do { \
		fprintf(stderr, "Error at line %d, file %s (%d) [%s]\n", \
				__LINE__, __FILE__, errno, strerror(errno)); \
		exit(1); \
	} while(0)
 
#define DEVMEM_PATH		"/dev/mem"
#define MAP_SIZE	4096UL
#define MAP_MASK	(MAP_SIZE - 1)

int read_flag = 1; // default behavior is read
int count_flag = 1; // default is to read only 1 element
int size_flag = sizeof(uint32_t); // default read size is 4 byte
off_t address_flag;
int write_flag;
unsigned long write_value;
int verbose_flag;

static inline void *fixup_addr(void *addr, size_t size)
{
#ifdef FORCE_STRICT_ALIGNMENT
	unsigned long aligned_addr = (unsigned long)addr;
	aligned_addr &= ~(size - 1);
	addr = (void *)aligned_addr;
#endif
	return addr;
}

void parse_command_line(int argc, char **argv)
{
	int c;
	int tmp;

	while ((c = getopt (argc, argv, "w:s:c:v")) != -1) {
		switch (c) {
			case 'w':
				read_flag = 0;
				write_flag = 1;
				write_value = strtoul(optarg, NULL, 0);
				break;
			
			case 'c':
				tmp = sscanf(optarg, "%d", &count_flag);
				if (tmp != 1) {
					fprintf (stderr, "Error: wrong length specified\n");
					abort();
				}
				break;
			
			case 's':
				tmp = sscanf(optarg, "%d", &size_flag);
				if (tmp != 1) {
					fprintf (stderr, "Error: unable to parse the specified size\n");
					abort();
				}
				if ((size_flag != sizeof(uint8_t)) && 
					(size_flag != sizeof(uint16_t)) && 
					(size_flag != sizeof(uint32_t)) && 
					(size_flag != sizeof(uint64_t))) {
					fprintf(stderr, "Error: invalid size %d\n", size_flag);
					abort();
				}
				break;
			
			case 'v':
				verbose_flag = 1;
				break;
			
			case '?':
				if (isprint(optopt))
					fprintf(stderr, "Error: unknown option `-%c'.\n", optopt);
				else
					fprintf(stderr, "Error: unknown option character `0x%x'.\n", optopt);
				abort();
			
			default:
				abort ();
		}
	}
	
	// the last option must be the address
	if (optind == argc) {
		fprintf (stderr, "Error: no address specified\n");
		abort();
	}
	
	address_flag = strtoul(argv[optind], NULL, 0);
	if (address_flag == 0) {
		fprintf (stderr, "Error: unable to parse the address (%lx)\n", address_flag);
		abort();
	}
	
	if (verbose_flag) {
		fprintf (stdout, "read_flag=%d \n"
					"address_flag=0x%lx \n"
					"count_flag=%d \n"
					"size_flag=%d \n"
					"write_flag=0x%x\n"
					"write_value=0x%lx\n", 
					read_flag, address_flag, count_flag, size_flag, write_flag, write_value);
	}
}

void read_single(void *map_base, off_t address, int size)
{
	void *virt_addr;
	unsigned long read_result;
	
	virt_addr = map_base + (address & MAP_MASK);
	virt_addr = fixup_addr(virt_addr, size);
	
	if (verbose_flag) {
		fprintf(stdout, "reading %d bytes from 0x%08lx (mapped to 0x%08p)\n", size, address, virt_addr);
	}
	
	switch (size_flag) {
		case sizeof(uint8_t):
			read_result = *((uint8_t *) virt_addr);
			fprintf(stdout, "0x%08lx: 0x%02hhx\n", address, (uint8_t)read_result);
			break;
		case sizeof(uint16_t):
			read_result = *((uint16_t *) virt_addr);
			fprintf(stdout, "0x%08lx: 0x%04hx\n", address, (uint16_t)read_result);
			break;
		case sizeof(uint32_t):
			read_result = *((uint32_t *) virt_addr);
			fprintf(stdout, "0x%08lx: 0x%08x\n", address, (uint32_t)read_result);
			break;
		case sizeof(uint64_t):
			read_result = *((uint64_t *) virt_addr);
			fprintf(stdout, "0x%08lx: 0x%016lx\n", address, (uint64_t)read_result);
			break;
	}
}

void write_single(void *map_base, off_t address, int size, unsigned long write_data)
{
	void *virt_addr;
	
	virt_addr = map_base + (address & MAP_MASK);
	virt_addr = fixup_addr(virt_addr, size);
	
	if (verbose_flag) {
		fprintf(stdout, "writing %d bytes (value 0x%lx) to 0x%08lx (mapped to 0x%08p)\n", size, write_data, address, virt_addr);
	}
	
	switch (size) {
		case sizeof(uint8_t):
			*((uint8_t *) virt_addr) = write_data;
			break;
		case sizeof(uint16_t):
			*((uint16_t *) virt_addr) = write_data;
			break;
		case sizeof(uint32_t):
			*((uint32_t *) virt_addr) = write_data;
			break;
		case sizeof(uint64_t):
			*((uint64_t *) virt_addr) = write_data;
			break;
	}
}

int main(int argc, char **argv) {
    int fd;
    void *map_base;
    off_t offset_in_page;
	
	parse_command_line(argc, argv);

    if ((fd = open(DEVMEM_PATH, O_RDWR | O_SYNC)) == -1) {
		fprintf(stderr, "Error: unable to open %s\n", DEVMEM_PATH);
		exit(1);
	}
	
    // write multiple data not supported for now
    if (write_flag && (count_flag > 1)) {
		fprintf(stderr, "Error: multiple writes are not accepted\n");
		exit(1);
	}
	
    /* Map one page */
    map_base = mmap(0, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, address_flag & ~MAP_MASK);
    if (map_base == (void *) -1) {
		fprintf(stderr, "Error: mmap failed\n");
		exit(1);
	}
	if (verbose_flag) {
		fprintf(stdout, "map_base==0x%08p\n", map_base);
	}
    
    if (read_flag) {
		while (count_flag > 0) {
			offset_in_page = address_flag & MAP_MASK;
			read_single(map_base, address_flag, size_flag);
			count_flag--;
			if (offset_in_page + size_flag >= MAP_SIZE) {
				fprintf(stderr, "Warning: border of mapping reached. Stopping here (%d remaining items)\n", count_flag);
				break;
			}
			address_flag += size_flag;
		}
	} else if (write_flag) {
		write_single(map_base, address_flag, size_flag, write_value);
	}
	
	if (munmap(map_base, MAP_SIZE) == -1) {
		fprintf(stderr, "Error: munmap failed\n");
		exit(1);
	}
    close(fd);
    
    return 0;
}
