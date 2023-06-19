#!/usr/bin/python
# vim: set noexpandtab:

from __future__ import division
from struct import *
import fcntl
import struct
import os
import errno

#
# Globals
#
DEVICE_PATH = '/dev/repeated'

#
# Utilities for calculating the IOCTL command codes.
#
sizeof = {
	'byte': calcsize('c'),
	'signed byte': calcsize('b'),
	'unsigned byte': calcsize('B'),
	'short': calcsize('h'),
	'unsigned short': calcsize('H'),
	'int': calcsize('i'),
	'unsigned int': calcsize('I'),
	'long': calcsize('l'),
	'unsigned long': calcsize('L'),
	'long long': calcsize('q'),
	'unsigned long long': calcsize('Q')
}

_IOC_NRBITS = 8
_IOC_TYPEBITS = 8
_IOC_SIZEBITS = 14
_IOC_DIRBITS = 2

_IOC_NRMASK = ((1 << _IOC_NRBITS)-1)
_IOC_TYPEMASK = ((1 << _IOC_TYPEBITS)-1)
_IOC_SIZEMASK = ((1 << _IOC_SIZEBITS)-1)
_IOC_DIRMASK = ((1 << _IOC_DIRBITS)-1)

_IOC_NRSHIFT = 0
_IOC_TYPESHIFT = (_IOC_NRSHIFT+_IOC_NRBITS)
_IOC_SIZESHIFT = (_IOC_TYPESHIFT+_IOC_TYPEBITS)
_IOC_DIRSHIFT = (_IOC_SIZESHIFT+_IOC_SIZEBITS)

_IOC_NONE = 0
_IOC_WRITE = 1
_IOC_READ = 2

def _IOC(dir, _type, nr, size):
	if type(_type) == str:
		_type = ord(_type)
		
	cmd_number = (((dir)  << _IOC_DIRSHIFT) | \
		((_type) << _IOC_TYPESHIFT) | \
		((nr)   << _IOC_NRSHIFT) | \
		((size) << _IOC_SIZESHIFT))

	return cmd_number

def _IO(_type, nr):
	return _IOC(_IOC_NONE, _type, nr, 0)

def _IOR(_type, nr, size):
	return _IOC(_IOC_READ, _type, nr, sizeof[size])

def _IOW(_type, nr, size):
	return _IOC(_IOC_WRITE, _type, nr, sizeof[size])

def main():
	"""Test the device driver"""
	
	#
	# Calculate the ioctl cmd number
	#
	MY_MAGIC = 'r'
	SET_STRING = _IOW(MY_MAGIC, 0, 'int')
	RESET = _IO(MY_MAGIC, 1)

	# Open the device file
	f = os.open(DEVICE_PATH, os.O_RDWR)
	
	# Set a key
	repeated = "abc"
	fcntl.ioctl(f, SET_STRING, repeated)

	# Write something
	message = "ignored"
	len_msg = len(message)
	os.write(f, message)

	# Read the repeated string up to len_msg bytes, even if we request more
	read_message = os.read(f, len_msg + 10)
	repeated_times = int(len_msg / len(repeated) + 1)

	# repeated string should be repeated ~repeated_times, but truncated after len_msg bytes
	expected_message = (repeated * repeated_times)[:len_msg]
	assert (message == expected_message)

	# Finaly close the device file
	os.close(f)


if __name__ == '__main__':
	main()
