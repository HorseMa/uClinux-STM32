obj-y           :=
obj-m           :=
obj-n           :=
obj-            :=

mod-subdirs     := 

O_TARGET := islmvc.o

export-objs     :=  isl_mgt.o islmvc_mgt.o 
list-multi	:=	
rcpci-objs	:=	

obj-$(CONFIG_ISIL_MVCDRIVER) += isl_mgt.o isl_wds.o islmvc_mgt.o islmvc_dev.o islmvc_blob.o
obj-$(CONFIG_ISIL_UAP) += isl_gen.o islpci_hotplug.o islpci_dev.o islpci_eth.o islpci_mgt.o isl_38xx.o

include $(TOPDIR)/Rules.make

clean:
	rm -f core *.o *.a *.s

