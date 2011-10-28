#Makefile at top of application tree
TOP = .
include $(TOP)/configure/CONFIG

#DIRS := configure caPutLogApp docs
DIRS := configure caPutLogApp

include $(TOP)/configure/RULES_TOP

upload:
	darcs push wwwcsr@www-csr.bessy.de:www/control/SoftDist/caPutLog/repo/caPutLog
	rsync -r html/* wwwcsr@www-csr.bessy.de:www/control/SoftDist/caPutLog
