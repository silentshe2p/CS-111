#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include "ext2_fs.h"

#define SUPERBLOCK_OFFSET 1024
#define GROUP_DESC_OFFSET 2048
#define REGULAR_FILE_MASK 	0x8000
#define DIRECTORY_MASK		0x4000
#define SYMBOLIC_LINK_MASK 	0xA000

const char REGULAR_FILE = 'f';
const char DIRECTORY = 'd';
const char SYMBOLIC_LINK = 's';
const char OTHER = '?';
const int SINGLE_INDIRECT = 1;
const int DOUBLE_INDIRECT = 2;
const int TRIPLE_INDIRECT = 3;

typedef struct ext2_super_block Superblock;
typedef struct ext2_group_desc GroupDesc;
typedef struct ext2_inode Inode;
typedef struct ext2_dir_entry Directory;

typedef struct ext2_info {
	unsigned int block_nums;
	unsigned int inode_nums;
	unsigned int block_size;
	unsigned int inode_size;
	unsigned int blocks_per_group;
	unsigned int inodes_per_group;
	unsigned int group_nums;	
} FSInfo;
FSInfo fi;

char *summary_file = "summary.csv";

void print_usage(void) {
	fprintf( stderr, "Usage: lab3a file_system_image\n" );
	exit(1);
}

void superblock_summary( int fs_fd, FILE *summary ) {
	Superblock *sb = malloc( sizeof(Superblock) );
	if( sb == NULL ) {
		fprintf( stderr, "malloc() superblock failed\n" );
		exit(2);
	}
	if( pread( fs_fd, sb, sizeof(Superblock), SUPERBLOCK_OFFSET ) == -1 ) {
		fprintf( stderr, "pread() superblock failed\n" );
		exit(2);
	}
	fi.block_nums = sb->s_blocks_count;
	fi.inode_nums = sb->s_inodes_count;
	fi.block_size = EXT2_MIN_BLOCK_SIZE << sb->s_log_block_size;
	fi.inode_size = sb->s_inode_size;
	fi.blocks_per_group = sb->s_blocks_per_group;
	fi.inodes_per_group = sb->s_inodes_per_group;
	unsigned int first_nr_inode = sb->s_first_ino;
	fi.group_nums = 1 + (sb->s_blocks_count-1) / sb->s_blocks_per_group;

	fprintf( summary, "%s,%d,%d,%d,%d,%d,%d,%d\n", "SUPERBLOCK", fi.block_nums, fi.inode_nums, fi.block_size, fi.inode_size, fi.blocks_per_group, fi.inodes_per_group, first_nr_inode );
	free(sb);
}

void read_directory( int fs_fd, uint32_t block_addr, int parent_inode, FILE *summary ) {
	int directory_offset = 0;
	// read until the end of block
	while( directory_offset < fi.block_size ) {
		Directory *dir = malloc(fi.block_size);
		// read rec_len first since entry size has variable size
		if( pread( fs_fd, &dir->rec_len, sizeof(dir->rec_len), block_addr * fi.block_size + directory_offset + sizeof(dir->inode) ) == -1 ) {
			fprintf( stderr, "pread() rec_len failed\n" );
			exit(2);
		}
		uint16_t rec_len = dir->rec_len;
		// now know rec_len, read into dir
		if( pread( fs_fd, dir, rec_len, block_addr * fi.block_size + directory_offset ) == -1 ) {
			fprintf( stderr, "pread() directory failed\n" );
			exit(2);
		}
		uint32_t inode_num = dir->inode;
		uint8_t name_len = dir->name_len;
		// stop reading when there is no more files
		if( name_len == 0 ) {
			free(dir);
			return;
		}		
		char *name = dir->name;
		name[name_len] = '\0';
		fprintf( summary, "%s,%d,%d,%d,%d,%d,'%s'\n", "DIRENT", parent_inode, directory_offset, inode_num, rec_len, name_len, name );
		directory_offset += rec_len;
		free(dir);
	}
}

// from seconds to the required format
void parse_time(uint32_t second, char *c) {
	time_t time = second;	
	struct tm *time_tm;
	time_tm = gmtime(&time);
	strftime( c, 21, "%D %T", time_tm );
}

void inode_summary( int fs_fd, int inode_i, unsigned int inode_table, FILE *summary ) {
	Inode *in = malloc(fi.inode_size);
	if( in == NULL ) {
		fprintf( stderr, "malloc() inode failed\n" );
		exit(2);
	}
	int local_inode = (inode_i - 1) % fi.inodes_per_group;
	if( pread( fs_fd, in, fi.inode_size, inode_table * fi.block_size + local_inode * fi.inode_size ) == -1 ) {
		fprintf( stderr, "pread() inode failed\n" );
		exit(2);
	}

	if( in->i_mode == 0 ) 
		return;
	unsigned mode = in->i_mode & 0x0FFF;

	char file_type;
	if( in->i_mode & REGULAR_FILE_MASK )
		file_type = REGULAR_FILE;
	else if( in->i_mode & DIRECTORY_MASK )
		file_type = DIRECTORY;
	else if( in->i_mode & SYMBOLIC_LINK_MASK )
		file_type = SYMBOLIC_LINK;
	else
		file_type = OTHER;

	uint16_t owner = in->i_uid;
	uint16_t group = in->i_gid;
	if( in->i_links_count == 0 )
		return;
	uint16_t links_count = in->i_links_count;

	char change_time[21];
	parse_time(in->i_ctime, change_time);
	char modification_time[21];
	parse_time(in->i_mtime, modification_time);		
	char access_time[21];
	parse_time(in->i_atime, access_time);

	uint32_t file_size = in->i_size;
	uint32_t block_nums = in->i_blocks;
	uint32_t *block_addr = in->i_block;
	fprintf( summary, "%s,%d,%c,%o,%d,%d,%d,%s,%s,%s,%d,%d", "INODE", inode_i, file_type, mode, owner, group, links_count, change_time, modification_time, access_time, file_size, block_nums );
	for( int i = 0; i < EXT2_N_BLOCKS; i++ ) {
		fprintf( summary, ",%d", block_addr[i] );
	}
	fprintf( summary, "\n" );

	// directory_entries_summary
	if( file_type == DIRECTORY ) {
		for( int i = 0; i < 12; i++ ) {
			// valid directory entries
			if( block_addr[i] != 0 )
				read_directory( fs_fd, block_addr[i], inode_i, summary );
		}
	}	

	// indirect_block_references_summary
	// single indirect block
	int logical_block = 11;
	if( block_addr[12] != 0 ) {
		uint32_t *first_lv_block_pointer = malloc(fi.block_size);
		if( first_lv_block_pointer == NULL ) {
			fprintf( stderr, "malloc() first_lv_block_pointer failed\n" );
			exit(2);
		}
		if( pread( fs_fd, first_lv_block_pointer, fi.block_size, block_addr[12] * fi.block_size ) == -1 ) {
			fprintf( stderr, "pread() first_lv_block_pointer failed\n" );
			exit(2);
		}
		// since this is an array of 4-bytes block numbers
		for( int i = 0; i < fi.block_size/4; i++ ) {
			if( first_lv_block_pointer[i] != 0 ) {
				logical_block++;
				fprintf( summary, "%s,%d,%d,%d,%d,%d\n", "INDIRECT", inode_i, SINGLE_INDIRECT, logical_block, block_addr[12], first_lv_block_pointer[i] );
			}
		}
	}

	// double indirect block
	if( block_addr[13] != 0 ) {
		uint32_t *first_lv_block_pointer = malloc(fi.block_size);
		if( first_lv_block_pointer == NULL ) {
			fprintf( stderr, "malloc() first_lv_block_pointer failed\n" );
			exit(2);
		}
		if( pread( fs_fd, first_lv_block_pointer, fi.block_size, block_addr[13] * fi.block_size ) == -1 ) {
			fprintf( stderr, "pread() first_lv_block_pointer failed\n" );
			exit(2);
		}
		for( int i = 0; i < fi.block_size/4; i++ ) {
			if( first_lv_block_pointer[i] != 0 ) {
				uint32_t *second_lv_block_pointer = malloc(fi.block_size);
				if( second_lv_block_pointer == NULL ) {
					fprintf( stderr, "malloc() second_lv_block_pointer failed\n" );
					exit(2);
				}
				if( pread( fs_fd, second_lv_block_pointer, fi.block_size, first_lv_block_pointer[i] * fi.block_size ) == -1 ) {
					fprintf( stderr, "pread() second_lv_block_pointer failed\n" );
					exit(2);
				}
				for( int j = 0; j < fi.block_size/4; j++ ) {
					if( second_lv_block_pointer[j] != 0 ) {
						logical_block++;
						fprintf( summary, "%s,%d,%d,%d,%d,%d\n", "INDIRECT", inode_i, DOUBLE_INDIRECT, logical_block, block_addr[13], second_lv_block_pointer[j] );				
					}
				}
			}
		}
	}

	// triple indirect block
	if( block_addr[14] != 0 ) {
		uint32_t *first_lv_block_pointer = malloc(fi.block_size);
		if( first_lv_block_pointer == NULL ) {
			fprintf( stderr, "malloc() first_lv_block_pointer failed\n" );
			exit(2);
		}
		if( pread( fs_fd, first_lv_block_pointer, fi.block_size, block_addr[14] * fi.block_size ) == -1 ) {
			fprintf( stderr, "pread() first_lv_block_pointer failed\n" );
			exit(2);
		}
		for( int i = 0; i < fi.block_size/4; i++ ) {
			if( first_lv_block_pointer[i] != 0 ) {
				uint32_t *second_lv_block_pointer = malloc(fi.block_size);
				if( second_lv_block_pointer == NULL ) {
					fprintf( stderr, "malloc() second_lv_block_pointer failed\n" );
					exit(2);
				}
				if( pread( fs_fd, second_lv_block_pointer, fi.block_size, first_lv_block_pointer[i] * fi.block_size ) == -1 ) {
					fprintf( stderr, "pread() second_lv_block_pointer failed\n" );
					exit(2);
				}
				for( int j = 0; j < fi.block_size/4; j++ ) {
					if( second_lv_block_pointer[j] != 0 ) {
						uint32_t *third_lv_block_pointer = malloc(fi.block_size);
						if( third_lv_block_pointer == NULL ) {
							fprintf( stderr, "malloc() third_lv_block_pointer failed\n" );
							exit(2);
						}
						if( pread( fs_fd, third_lv_block_pointer, fi.block_size, second_lv_block_pointer[j] * fi.block_size ) == -1 ) {
							fprintf( stderr, "pread() third_lv_block_pointer failed\n" );
							exit(2);
						}
						for( int k = 0; k < fi.block_size/4; k++ ) {
							if( third_lv_block_pointer[i] != 0 ) {
								logical_block++;
								fprintf( summary, "%s,%d,%d,%d,%d,%d\n", "INDIRECT", inode_i, TRIPLE_INDIRECT, logical_block, block_addr[14], third_lv_block_pointer[k] );				
							}
						}
					}
				}
			}
		}
	}
	free(in);
}

void group_summary( int fs_fd, FILE *summary ) {
	GroupDesc *gd = malloc( sizeof(GroupDesc) );
	if( gd == NULL ) {
		fprintf( stderr, "malloc() group descriptor failed\n" );
		exit(2);
	}
	if( pread( fs_fd, gd, fi.group_nums * sizeof(GroupDesc), GROUP_DESC_OFFSET ) == -1 ) {
		fprintf( stderr, "pread() group descriptor failed\n" );
		exit(2);
	}

	for( int i = 0; i < fi.group_nums; i++ ) {
		unsigned int free_blocks_nums = gd[i].bg_free_blocks_count;
		unsigned int free_inodes_nums = gd[i].bg_free_inodes_count;
		unsigned int free_block_bitmap = gd[i].bg_block_bitmap;
		unsigned int free_inode_bitmap = gd[i].bg_inode_bitmap;
		unsigned int first_block_inode = gd[i].bg_inode_table;
		fprintf( summary, "%s,%d,%d,%d,%d,%d,%d,%d,%d\n", "GROUP", i, (fi.block_nums <= fi.blocks_per_group) ? fi.block_nums : fi.blocks_per_group, fi.inodes_per_group, free_blocks_nums, free_inodes_nums, free_block_bitmap, free_inode_bitmap, first_block_inode );
		
		// free_block_summary
		uint8_t *block_bitmap = malloc(fi.block_size);
		if( block_bitmap == NULL ) {
			fprintf( stderr, "malloc() block bitmap failed\n" );
			exit(2);
		}
		if( pread( fs_fd, block_bitmap, fi.block_size, free_block_bitmap * fi.block_size ) == -1 ) {
			fprintf( stderr, "pread() block bitmap failed\n" );
			exit(2);
		}

		for( int j = 0, block_i = 1; j < fi.block_size; j++ ) {
			if( block_i > fi.blocks_per_group ) {
				break;
			}				
			uint8_t byte = block_bitmap[j];
			// iterate through 8 bits
			for( int k = 0; k < 8; k++, block_i++ ) {			
				// bit = 0 meaning block (block_i) is free
				if( !(byte & (1 << k)) )
					fprintf( summary, "%s,%d\n", "BFREE", block_i );
			}
		}
		free(block_bitmap);

		// free_inode_summary
		uint8_t *inode_bitmap = malloc(fi.block_size);
		if( inode_bitmap == NULL ) {
			fprintf( stderr, "malloc() inode bitmap failed\n" );
			exit(2);
		}
		if( pread( fs_fd, inode_bitmap, fi.block_size, free_inode_bitmap * fi.block_size ) == -1 ) {
			fprintf( stderr, "pread() inode bitmap failed\n" );
			exit(2);
		}

		for( int j = 0, inode_i = 1; j < fi.block_size; j++ ) {
			if( inode_i > fi.inodes_per_group ) {
				break;
			}			
			uint8_t byte = inode_bitmap[j];
			// iterate through 8 bits
			for( int k = 0; k < 8; k++, inode_i++ ) {
				// inode_i is free
				if( !(byte & (1 << k)) )
					fprintf( summary, "%s,%d\n", "IFREE", inode_i );
			}
		}

		// inode_summary
		for( int j = 0, inode_i = 1; j < fi.block_size; j++ ) {
			if( inode_i > fi.inodes_per_group ) {
				break;
			}		
			uint8_t byte = inode_bitmap[j];
			for( int k = 0; k < 8; k++, inode_i++ ) {				
				// inode_i is not free
				if( byte & (1 << k) )
					inode_summary(fs_fd, inode_i, first_block_inode, summary );
			}
		}
		free(inode_bitmap);
	}
	free(gd);
}

int main( int argc, char **argv ) {
	if( argc !=2 )
		print_usage();
	char *fs_img = argv[1];
	int fs_fd = open(fs_img, O_RDONLY);
	if( fs_fd < 0 ) {
		fprintf( stderr, "Error opening image\n" );
		exit(2);
	}

	FILE *summary = fopen("summary.csv", "w");
	superblock_summary(fs_fd, summary);
	group_summary(fs_fd, summary);
	fclose(summary);
	return 0;
}