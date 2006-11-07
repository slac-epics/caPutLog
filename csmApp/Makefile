TOP=..
include $(TOP)/configure/CONFIG

LIBRARY_IOC = csmbase

csmbase_SRCS = csmbase.c
INC = csmbase.h

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

$(COMMON_DIR)/csmbase.h: csmbase.c
	hgen.pl -c -h $@ $<
