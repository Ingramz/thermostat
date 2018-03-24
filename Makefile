PROGRAM=thermostat

# Sonoff special
FLASH_MODE ?= dout
FLASH_SIZE ?= 8

EXTRA_CFLAGS=-DLWIP_HTTPD_CGI=1 -DLWIP_HTTPD_SSI=1 -I./fsdata

#Enable debugging
#EXTRA_CFLAGS+=-DLWIP_DEBUG=1 -DHTTPD_DEBUG=LWIP_DBG_ON

EXTRA_COMPONENTS=extras/mbedtls extras/httpd

include ../../common.mk

html:
	@echo "Generating fsdata.."
	cd fsdata && ./makefsdata
