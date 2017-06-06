# NAME: Bach Hoang
# ID: 104737449
# EMAIL: ko.wing.bird@gmail.com

import sys
import csv

SUPER = "SUPERBLOCK"
GROUP = "GROUP"
BFREE = "BFREE"
IFREE = "IFREE"
INODE = "INODE"
DIR = "DIRENT"
IND = "INDIRECT"
SGL_BLK = "BLOCK"
IND_BLK = "INDIRECT BLOCK"
DBL_BLK = "DOUBLE INDIRECT BLOCK"
TRP_BLK = "TRIPPLE INDIRECT BLOCK"
DIRECTORY = "d"
CURRENT_DIR = "'.'"
PARENT_DIR = "'..'"
EXT2_ROOT_INO = 2
BLOCKS_PER_INDIRECT_BLOCK = 256
BLOCKS_PER_DOUBLY_INDIRECT_BLOCK = 65536


POINTERS_IN_INODE = 15

# assuming one group
class Ext2_info:
	def __init__(self):
		self.blocks_count = 0
		self.inodes_count = 0
		self.block_size = 0
		self.inode_size = 0
		self.inodes_per_group = 0
		self.free_inodes_count = 0
		self.inode_table = 0
		self.free_blocks = []
		self.free_inodes = []
		self.inferred_free_inodes = []
		self.inodes = []

class Block:
	def __init__(self, block_num):
		self.block_num = block_num
		self.ref_inodes = []

class Inode:
	def __init__(self, inode_num, link_count, is_directory=False):
		self.inode_num = inode_num
		self.link_count = link_count
		self.ref_entries = []
		self.ref_blocks = []
		self.ref_indirect_blocks = []
		self.is_directory = is_directory
	def __eq__(self,other):
		# explaination for this special case when link_count == -1 is in check_inode()
		if self.link_count == -1: 
			return self.inode_num == other.inode_num and other.link_count != 0
		else:
			return self.inode_num == other.inode_num and self.link_count == other.link_count

class Entry:
	def __init__(self, ref_inode_num, parent_inode_num, name):
		self.ref_inode_num = ref_inode_num
		self.parent_inode_num = parent_inode_num
		self.name = name

def parse_csv(fs_info, csv_file):
	with open(csv_file, 'rb') as csvfile:
		reader = csv.reader(csvfile, delimiter=',')
		for row in reader:
			if row[0] == SUPER:
				fs_info.block_size = int(row[3])
				fs_info.inode_size = int(row[4])
				fs_info.inodes_per_group = int(row[6])
			# assuming one group
			if row[0] == GROUP:
				fs_info.blocks_count = int(row[2])
				fs_info.inodes_count = int(row[3])				
				fs_info.free_inodes_count = int(row[5])
				fs_info.inode_table = int(row[8])
				for i in xrange(fs_info.inodes_count, fs_info.inodes_count-fs_info.free_inodes_count, -1):
					fs_info.inferred_free_inodes.append(i)
			if row[0] == BFREE:
				block = Block(int(row[1]))
				fs_info.free_blocks.append(block)
			if row[0] == IFREE:
				inode = Inode(int(row[1]), 0)
				fs_info.free_inodes.append(inode)
			if row[0] == INODE:
				inode = Inode(int(row[1]), int(row[6]), (True if row[2] == DIRECTORY else False))
				for i in xrange(12, 27):
					inode.ref_blocks.append(int(row[i]))
				fs_info.inodes.append(inode)

	# second read to make sure needed info has already been read
	with open(csv_file, 'rb') as csvfile:
		reader = csv.reader(csvfile, delimiter=',')
		for row in reader:	
			if row[0] == DIR:
				if int(row[3]) > fs_info.inodes_count or int(row[3]) < 1:
					sys.stdout.write("DIRECTORY INODE %d NAME %s INVALID INODE %d\n" % (int(row[1]), row[6], int(row[3])))
				elif int(row[3]) in fs_info.inferred_free_inodes:
					sys.stdout.write("DIRECTORY INODE %d NAME %s UNALLOCATED INODE %d\n" % (int(row[1]), row[6], int(row[3])))
				else:
					for inode in fs_info.inodes:
						if inode.inode_num == int(row[3]):
							entry = Entry(int(row[3]), int(row[1]), row[6])
							inode.ref_entries.append(entry)
			if row[0] == IND:
				for inode in fs_info.inodes:
					if inode.inode_num == int(row[1]):
						block = Block(int(row[5]))
						inode.ref_indirect_blocks.append(block)

# find type of block based on its location
def parse_block_type(pointer_location):
	if POINTERS_IN_INODE - pointer_location == 1:
		return TRP_BLK
	elif POINTERS_IN_INODE - pointer_location == 2:
		return DBL_BLK
	elif POINTERS_IN_INODE - pointer_location == 3:
		return IND_BLK
	else:
		return SGL_BLK

def inode_with_duplicate_ref_block(fs_info, block_num):
	for inode in fs_info.inodes:
		pointer_location = 0
		for rb in inode.ref_blocks:
			if rb == block_num:
				return inode.inode_num, parse_block_type(pointer_location), pointer_location
			pointer_location += 1

def block_offset(pointer_location):
	if pointer_location < 0 or pointer_location > 14:
		sys.stderr.write("Invalid pointer location\n")
		sys.exit(1)
	if pointer_location <= 12:
		return pointer_location
	elif pointer_location == 13:
		return 12 + BLOCKS_PER_INDIRECT_BLOCK
	elif pointer_location == 14:
		return 12 + BLOCKS_PER_INDIRECT_BLOCK + BLOCKS_PER_DOUBLY_INDIRECT_BLOCK

def check_inode(fs_info):
	for inode in fs_info.free_inodes:
		if inode.inode_num < fs_info.inodes_count - fs_info.free_inodes_count:
			sys.stdout.write("ALLOCATED INODE %d ON FREELIST\n" % inode.inode_num)
		if inode.inode_num in fs_info.inferred_free_inodes:
			fs_info.inferred_free_inodes.remove(inode.inode_num)
	if len(fs_info.inferred_free_inodes) != 0:
		for i in xrange(len(fs_info.inferred_free_inodes)):
			################################################################################################
			# i make this special case passing link_count as -1 to check and if there is an inode in fs_info.inodes
			# with the a certain inode_num but link_count can be anything different than 0 as I don't want a long loop
			# to solve my question on piazza that never got answered: https://piazza.com/class/j119yaxomjl2qb?cid=691
			if Inode(fs_info.inferred_free_inodes[i], -1) in fs_info.inodes:
				continue
			################################################################################################
			else:
				sys.stdout.write("UNALLOCATED INODE %d NOT ON FREELIST\n" % fs_info.inferred_free_inodes[i])

def check_block(fs_info):
	inodes_per_block = fs_info.block_size / fs_info.inode_size
	inode_table_size = fs_info.inodes_per_group / inodes_per_block
	allocated_blocks = []
	# check invalid and reserved
	for inode in fs_info.inodes:
		# add ref_indirect_blocks to allocated_blocks if there is any
		if len(inode.ref_indirect_blocks) != 0:
			for i in xrange(len(inode.ref_indirect_blocks)):
				allocated_blocks.append(inode.ref_indirect_blocks[i].block_num)			
		pointer_location = 0
		for rb in inode.ref_blocks:
			if rb == 0:
				pointer_location += 1
				continue
			if rb > fs_info.blocks_count:
				blk_type = parse_block_type(pointer_location)
				sys.stdout.write("INVALID %s %d IN INODE %d AT OFFSET %d\n" % (blk_type, rb, inode.inode_num, block_offset(pointer_location)))
			elif rb < fs_info.inode_table + inode_table_size:
				blk_type = parse_block_type(pointer_location)
				sys.stdout.write("RESERVED %s %d IN INODE %d AT OFFSET %d\n" % (blk_type, rb, inode.inode_num, block_offset(pointer_location)))
			else:
				if rb in allocated_blocks:
					existed, existed_blk_type, existed_pointer_location = inode_with_duplicate_ref_block(fs_info, rb)
					sys.stdout.write("DUPLICATE %s %d IN INODE %d AT OFFSET %d\n" % (existed_blk_type, rb, existed, block_offset(existed_pointer_location)))
					blk_type = parse_block_type(pointer_location)
					sys.stdout.write("DUPLICATE %s %d IN INODE %d AT OFFSET %d\n" % (blk_type, rb, inode.inode_num, block_offset(pointer_location)))
					pointer_location += 1
					continue
				allocated_blocks.append(rb)
			pointer_location += 1
	# find unallocated blocks
	free_blocks = []
	for i in xrange(fs_info.inode_table + inode_table_size, fs_info.blocks_count):
		if i in allocated_blocks:
			continue
		free_blocks.append(i)
	# check freelist
	for block in fs_info.free_blocks:
		if block.block_num in allocated_blocks:
			sys.stdout.write("ALLOCATED BLOCK %d ON FREELIST\n" % block.block_num)
		if block.block_num in free_blocks:
			free_blocks.remove(block.block_num)
	if len(free_blocks) != 0:
		for block in free_blocks:
			sys.stdout.write("UNREFERENCED BLOCK %d\n" % block)

def check_directory(fs_info):
	for inode in fs_info.inodes:
		if inode.link_count != len(inode.ref_entries):
			sys.stdout.write("INODE %d HAS %d LINKS BUT LINKCOUNT IS %d\n" % (inode.inode_num, len(inode.ref_entries), inode.link_count))
		if inode.is_directory == True:
			for entry in inode.ref_entries:
				if entry.name == CURRENT_DIR and entry.ref_inode_num != entry.parent_inode_num:
					sys.stdout.write("DIRECTORY INODE %d NAME %s LINK TO INODE %d SHOULD BE %d\n" % (entry.parent_inode_num, CURRENT_DIR, entry.ref_inode_num, entry.parent_inode_num))
				if entry.name == PARENT_DIR and entry.ref_inode_num != EXT2_ROOT_INO:
					sys.stdout.write("DIRECTORY INODE %d NAME %s LINK TO INODE %d SHOULD BE %d\n" % (entry.parent_inode_num, PARENT_DIR, entry.ref_inode_num, EXT2_ROOT_INO))

if __name__ == "__main__":
	if len(sys.argv) != 2:
		sys.stderr.write("Incorrect number of argument\n")
		sys.exit(1)
	fs_info = Ext2_info()
	parse_csv(fs_info, sys.argv[1])
	check_inode(fs_info)
	check_block(fs_info)
	check_directory(fs_info)
