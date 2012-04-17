/* Linux host side PCI passthrough support
 * Jack Lange <jacklange@cs.pitt.edu>, 2012
 */

#include <linux/pci.h>
#include <linux/iommu.h>
#include <linux/interrupt.h>
#include <linux/version.h>


#define PCI_HDR_SIZE 256


static int setup_hw_pci_dev(struct host_pci_device * host_dev) {
    int ret = 0;
    struct pci_dev * dev = NULL;
    struct v3_host_pci_dev * v3_dev = &(host_dev->v3_dev);

    dev = pci_get_bus_and_slot(host_dev->hw_dev.bus,
			       host_dev->hw_dev.devfn);


    if (dev == NULL) {
	printk("Could not find HW pci device (bus=%d, devfn=%d)\n", 
	       host_dev->hw_dev.bus, host_dev->hw_dev.devfn); 
	return -1;
    }

    // record pointer in dev state
    host_dev->hw_dev.dev = dev;

    host_dev->hw_dev.intx_disabled = 1;
    spin_lock_init(&(host_dev->hw_dev.intx_lock));

    if (pci_enable_device(dev)) {
	printk("Could not enable Device\n");
	return -1;
    }
    
    ret = pci_request_regions(dev, "v3vee");
    if (ret != 0) {
	printk("Could not reservce PCI regions\n");
	return -1;
    }


    pci_reset_function(host_dev->hw_dev.dev);
    pci_save_state(host_dev->hw_dev.dev);


    {
	int i = 0;
	for (i = 0; i < DEVICE_COUNT_RESOURCE; i++) {
	    printk("Resource %d\n", i);
	    printk("\tflags = 0x%lx\n", pci_resource_flags(dev, i));
	    printk("\t name=%s, start=%lx, size=%d\n", 
		   host_dev->hw_dev.dev->resource[i].name, (uintptr_t)pci_resource_start(dev, i),
		   (u32)pci_resource_len(dev, i));

	}

	printk("Rom BAR=%d\n", dev->rom_base_reg);
    }

    /* Cache first 6 BAR regs */
    {
	int i = 0;

	for (i = 0; i < 6; i++) {
	    struct v3_host_pci_bar * bar = &(v3_dev->bars[i]);
	    unsigned long flags;
	    
	    bar->size = pci_resource_len(dev, i);
	    bar->addr = pci_resource_start(dev, i);
	    flags = pci_resource_flags(dev, i);

	    if (flags & IORESOURCE_IO) {
		bar->type = PT_BAR_IO;
	    } else if (flags & IORESOURCE_MEM) {
		if (flags & IORESOURCE_MEM_64) {
		    printk("ERROR: 64 Bit BARS not yet supported\n");
		    bar->type = PT_BAR_NONE;
		} else if (flags & IORESOURCE_DMA) {
		    bar->type = PT_BAR_MEM24;
		} else {
		    bar->type = PT_BAR_MEM32;
		}
		
		bar->cacheable = ((flags & IORESOURCE_CACHEABLE) != 0);
		bar->prefetchable = ((flags & IORESOURCE_PREFETCH) != 0);

	    } else {
		bar->type = PT_BAR_NONE;
	    }
	}
    }

    /* Cache expansion rom bar */
    {
	struct resource * rom_res = &(dev->resource[PCI_ROM_RESOURCE]);
	int rom_size = pci_resource_len(dev, PCI_ROM_RESOURCE);

	if (rom_size > 0) {
	    unsigned long flags;

	    v3_dev->exp_rom.size = rom_size;
	    v3_dev->exp_rom.addr = pci_resource_start(dev, PCI_ROM_RESOURCE);
	    flags = pci_resource_flags(dev, PCI_ROM_RESOURCE);

	    v3_dev->exp_rom.type = PT_EXP_ROM;

	    v3_dev->exp_rom.exp_rom_enabled = rom_res->flags & IORESOURCE_ROM_ENABLE;
	}
    }

    /* Cache entire configuration space */
    {
	int m = 0;

	// Copy the configuration space to the local cached version
	for (m = 0; m < PCI_HDR_SIZE; m += 4) {
	    pci_read_config_dword(dev, m, (u32 *)&(v3_dev->cfg_space[m]));
	}
    }


    /* HARDCODED for now but this will need to depend on IOMMU support detection */
    if (iommu_found()) {
	printk("Setting host PCI device (%s) as IOMMU\n", host_dev->name);
	v3_dev->iface = IOMMU;
    } else {
	printk("Setting host PCI device (%s) as SYMBIOTIC\n", host_dev->name);
	v3_dev->iface = SYMBIOTIC;
    }

    return 0;

}



static irqreturn_t host_pci_intx_irq_handler(int irq, void * priv_data) {
    struct host_pci_device * host_dev = priv_data;

    //   printk("Host PCI IRQ handler (%d)\n", irq);

    spin_lock(&(host_dev->hw_dev.intx_lock));
    disable_irq_nosync(irq);
    host_dev->hw_dev.intx_disabled = 1;
    spin_unlock(&(host_dev->hw_dev.intx_lock));

    V3_host_pci_raise_irq(&(host_dev->v3_dev), 0);

    return IRQ_HANDLED;
}



static irqreturn_t host_pci_msi_irq_handler(int irq, void * priv_data) {
    struct host_pci_device * host_dev = priv_data;
    //    printk("Host PCI MSI IRQ Handler (%d)\n", irq);

    V3_host_pci_raise_irq(&(host_dev->v3_dev), 0);

    return IRQ_HANDLED;
}

static irqreturn_t host_pci_msix_irq_handler(int irq, void * priv_data) {
    struct host_pci_device * host_dev = priv_data;
    int i = 0;
    
    //    printk("Host PCI MSIX IRQ Handler (%d)\n", irq);
    
    // find vector index
    for (i = 0; i < host_dev->hw_dev.num_msix_vecs; i++) {
	if (irq == host_dev->hw_dev.msix_entries[i].vector) {
	    V3_host_pci_raise_irq(&(host_dev->v3_dev), i);
	} else {
	    printk("Error Could not find matching MSIX vector for IRQ %d\n", irq);
	}
    }    
    return IRQ_HANDLED;
}


static int hw_pci_cmd(struct host_pci_device * host_dev, host_pci_cmd_t cmd, u64 arg) {
    //struct v3_host_pci_dev * v3_dev = &(host_dev->v3_dev);
    struct pci_dev * dev = host_dev->hw_dev.dev;

    switch (cmd) {
	case HOST_PCI_CMD_DMA_DISABLE:
	    printk("Passthrough PCI device disabling BMDMA\n");
	    pci_clear_master(host_dev->hw_dev.dev);
	    break;
	case HOST_PCI_CMD_DMA_ENABLE:
	    printk("Passthrough PCI device Enabling BMDMA\n");
	    pci_set_master(host_dev->hw_dev.dev);
	    break;

	case HOST_PCI_CMD_INTX_DISABLE:
	    printk("Passthrough PCI device disabling INTx IRQ\n");

	    disable_irq(dev->irq);
	    free_irq(dev->irq, (void *)host_dev);

	    break;
	case HOST_PCI_CMD_INTX_ENABLE:
	    printk("Passthrough PCI device Enabling INTx IRQ\n");
	
	    if (request_threaded_irq(dev->irq, NULL, host_pci_intx_irq_handler, 
				     IRQF_ONESHOT, "V3Vee_Host_PCI_INTx", (void *)host_dev)) {
		printk("ERROR Could not assign IRQ to host PCI device (%s)\n", host_dev->name);
	    }

	    break;

	case HOST_PCI_CMD_MSI_DISABLE:
	    printk("Passthrough PCI device Disabling MSIs\n");

	    disable_irq(dev->irq);
	    free_irq(dev->irq, (void *)host_dev);

	    pci_disable_msi(dev);

	    break;
	case HOST_PCI_CMD_MSI_ENABLE:
	    printk("Passthrough PCI device Enabling MSI\n");
	    
	    if (!dev->msi_enabled) {
		pci_enable_msi(dev);

		if (request_irq(dev->irq, host_pci_msi_irq_handler, 
				0, "V3Vee_host_PCI_MSI", (void *)host_dev)) {
		    printk("Error Requesting IRQ %d for Passthrough MSI IRQ\n", dev->irq);
		}
	    }

	    break;



	case HOST_PCI_CMD_MSIX_ENABLE: {
	    int i = 0;
	    
	    printk("Passthrough PCI device Enabling MSIX\n");
	    host_dev->hw_dev.num_msix_vecs = arg;;
	    host_dev->hw_dev.msix_entries = kcalloc(host_dev->hw_dev.num_msix_vecs, 
						    sizeof(struct msix_entry), GFP_KERNEL);
	    
	    for (i = 0; i < host_dev->hw_dev.num_msix_vecs; i++) {
		host_dev->hw_dev.msix_entries[i].entry = i;
	    }
	    
	    pci_enable_msix(dev, host_dev->hw_dev.msix_entries, 
			    host_dev->hw_dev.num_msix_vecs);
	    
	    for (i = 0; i < host_dev->hw_dev.num_msix_vecs; i++) {
		if (request_irq(host_dev->hw_dev.msix_entries[i].vector, 
				host_pci_msix_irq_handler, 
				0, "V3VEE_host_PCI_MSIX", (void *)host_dev)) {
		    printk("Error requesting IRQ %d for Passthrough MSIX IRQ\n", 
			   host_dev->hw_dev.msix_entries[i].vector);
		}
	    }

	    break;
	}

	case HOST_PCI_CMD_MSIX_DISABLE: {
	    int i = 0;

	    printk("Passthrough PCI device Disabling MSIX\n");
	    
	    for (i = 0; i < host_dev->hw_dev.num_msix_vecs; i++) {
		disable_irq(host_dev->hw_dev.msix_entries[i].vector);
	    }

	    for (i = 0; i < host_dev->hw_dev.num_msix_vecs; i++) {
		free_irq(host_dev->hw_dev.msix_entries[i].vector, (void *)host_dev);
	    }

	    host_dev->hw_dev.num_msix_vecs = 0;
	    kfree(host_dev->hw_dev.msix_entries);

	    pci_disable_msix(dev);

	    break;
	}
	default:
	    printk("Error: unhandled passthrough PCI command: %d\n", cmd);
	    return -1;
	   
    }

    return 0;
}


static int hw_ack_irq(struct host_pci_device * host_dev, u32 vector) {
    struct pci_dev * dev = host_dev->hw_dev.dev;
    unsigned long flags;

    //    printk("Acking IRQ vector %d\n", vector);

    spin_lock_irqsave(&(host_dev->hw_dev.intx_lock), flags);
    //    printk("Enabling IRQ %d\n", dev->irq);
    enable_irq(dev->irq);
    host_dev->hw_dev.intx_disabled = 0;
    spin_unlock_irqrestore(&(host_dev->hw_dev.intx_lock), flags);
    
    return 0;
}




static int reserve_hw_pci_dev(struct host_pci_device * host_dev, void * v3_ctx) {
    int ret = 0;
    unsigned long flags;
    struct v3_host_pci_dev * v3_dev = &(host_dev->v3_dev);
    struct pci_dev * dev = host_dev->hw_dev.dev;

    spin_lock_irqsave(&lock, flags);
    if (host_dev->hw_dev.in_use == 0) {
	host_dev->hw_dev.in_use = 1;
    } else {
	ret = -1;
    }
    spin_unlock_irqrestore(&lock, flags);


    if (v3_dev->iface == IOMMU) {
	struct v3_guest_mem_region region;
	int flags = 0;

	host_dev->hw_dev.iommu_domain = iommu_domain_alloc();

	if (V3_get_guest_mem_region(v3_ctx, &region) == -1) {
	    printk("Error getting VM memory region for IOMMU support\n");
	    return -1;
	}
	
	printk("Memory region: start=%p, end=%p\n", (void *)region.start, (void *)region.end);


	flags = IOMMU_READ | IOMMU_WRITE; // Need to see what IOMMU_CACHE means
	
	/* This version could be wrong */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,38) 
	// Guest VAs start at zero and go to end of memory
	iommu_map_range(host_dev->hw_dev.iommu_domain, 0, region.start, (region.end - region.start), flags);
#else 
	/* Linux actually made the interface worse... Now you can only map memory in powers of 2 (meant to only be pages...) */
	{	
	    u64 size = region.end - region.start;
	    u32 page_size = 512 * 4096; // assume large 64bit pages (2MB)
	    u64 dpa = 0; // same as gpa
	    u64 hpa = region.start;

	    do {
		if (size < page_size) {
		    page_size = 4096; // less than a 2MB granularity, so we switch to small pages (4KB)
		}
		
		printk("Mapping IOMMU region dpa=%p hpa=%p (size=%d)\n", (void *)dpa, (void *)hpa, page_size);

		if (iommu_map(host_dev->hw_dev.iommu_domain, dpa, hpa, 
			      get_order(page_size), flags)) {
		    printk("ERROR: Could not map sub region (DPA=%p) (HPA=%p) (order=%d)\n", 
			   (void *)dpa, (void *)hpa, get_order(page_size));
		    break;
		}

		hpa += page_size;
		dpa += page_size;

		size -= page_size;
	    } while (size);
	}
#endif

	if (iommu_attach_device(host_dev->hw_dev.iommu_domain, &(dev->dev))) {
	    printk("ERROR attaching host PCI device to IOMMU domain\n");
	}

    }


    printk("Requesting Threaded IRQ handler for IRQ %d\n", dev->irq);
    // setup regular IRQs until advanced IRQ mechanisms are enabled
    if (request_threaded_irq(dev->irq, NULL, host_pci_intx_irq_handler, 
			     IRQF_ONESHOT, "V3Vee_Host_PCI_INTx", (void *)host_dev)) {
	printk("ERROR Could not assign IRQ to host PCI device (%s)\n", host_dev->name);
    }



    
    return ret;
}



static int write_hw_pci_config(struct host_pci_device * host_dev, u32 reg, void * data, u32 length) {
    struct pci_dev * dev = host_dev->hw_dev.dev;

    if (reg < 64) {
	return 0;
    }
	
    if (length == 1) {
	pci_write_config_byte(dev, reg, *(u8 *)data);
    } else if (length == 2) {
	pci_write_config_word(dev, reg, *(u16 *)data);
    } else if (length == 4) {
	pci_write_config_dword(dev, reg, *(u32 *)data);
    } else {
	printk("Invalid length of host PCI config update\n");
	return -1;
    }
    
    return 0;
}



static int read_hw_pci_config(struct host_pci_device * host_dev, u32 reg, void * data, u32 length) {
    struct pci_dev * dev = host_dev->hw_dev.dev;

	
    if (length == 1) {
	pci_read_config_byte(dev, reg, data);
    } else if (length == 2) {
	pci_read_config_word(dev, reg, data);
    } else if (length == 4) {
	pci_read_config_dword(dev, reg, data);
    } else {
	printk("Invalid length of host PCI config read\n");
	return -1;
    }


    return 0; 
}
