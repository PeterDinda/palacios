/*
 * defs.h
 *
 *  Created on: Oct 4, 2012
 *      Author: Abhinav Kannan
 */

#ifndef DEFS_H_
#define DEFS_H_

const char* STATUS_BAR_MSG_READY = "Ready";

const char* TITLE_MAIN_WINDOW = "Palacios";
const char* TITLE_DOCK_TELEMETRY = "Kernel messages";
const char* TITLE_DOCK_VM_LIST = "List of VMs";

const char* MENU_FILE = "File";
const char* MENU_VIEW = "View";
const char* MENU_VM = "VM";
const char* MENU_HELP = "Help";

const char* FILE_MENU_NEW_VM = "New VM";
const char* NEW_VM_STATUS_TIP = "Create a new virtual machine";
const char* FILE_MENU_EXIT = "Exit";
const char* EXIT_STATUS_TIP = "Exit Palacios VMM";

const char* VM_MENU_START = "Start VM";
const char* VM_MENU_STOP = "Stop VM";
const char* VM_MENU_PAUSE = "Pause VM";
const char* VM_MENU_RESTART = "Restart VM";
const char* VM_MENU_REMOVE = "Delete VM";
const char* VM_MENU_ACTIVATE = "Activate VM";
const char* VM_MENU_RELOAD = "Reload VMs";

const char* VM_STOP_WARNING_MESSAGE = "You are about to stop a running virtual machine. Please stop all executing processes"
		"within the virtual machine to insure safe termination of VM. Do you want to continue?";
const char* VM_DELETE_WARNING_MESSAGE = "Are you sure you want to delete this VM?";
const char* DELETE_RUNNING_VM_ERROR = "This VM is currently running! Please stop the VM before deleting";

const char* HELP_MENU_ABOUT = "About Palacios";
const char* ABOUT_PALACIOS =
		"Palacios is a virtual machine monitor (VMM) "
				"that is available for public use as a community resource. Palacios is highly configurable "
				"and designed to be embeddable into different host operating systems, such as Linux and the "
				"Kitten lightweight kernel. Palacios is a non-paravirtualized VMM that makes extensive use of "
				"the virtualization extensions in modern Intel and AMD x86 processors. "
				"Palacios is a compact codebase that has been designed to be easy to understand and readily "
				"configurable for different environments. It is unique in being designed to be embeddable into "
				"other OSes instead of being implemented in the context of a specific OS. Palacios is distributed under the BSD license."

				"\nPalacios is part of the V3VEE Project";

const char* XTERM_CMD = "/usr/bin/xterm";

const char* FILE_VM_LIST = "virtual_machines_list.txt";

const char* TAG_VM = "vm";

const char* ERROR_TELEMETRY = "Telemetry information currently unavailable";

const char* LABEL_ACTIVE_INVENTORY = "Active Inventory";
const char* LABEL_ACTIVE_NOT_INVENTORY = "Not in inventory";
const char* LABEL_INACTIVE_INVENTORY = "Inactive Inventory";

const char* ERROR_SETUP_MODULE_INSTALL = "Kernel Module not installed";
const char* ERROR_SETUP_MODULE_INSTALL_FAILED = "Kernel module not installed correctly";
const char* ERROR_SETUP_MEMORY = "Memory not intialized";

const char* ERROR_APP_CLOSE = "There are running VMs in the current session. Stop or pause the VMs before exiting";
const char* ERROR_VM_RUNNING = "VM is already running";
const char* ERROR_UPDATE_VM_STATE = "Error could not update VM state";
const char* ERROR_RUN_ACTIVE_NOT_INVENTORY = "VM instance exists on the system but has not been added to the inventory. Activate VM to proceed";
const char* ERROR_RUN_INACTIVE_INVENTORY = "VM instance has not been activated";
const char* ERROR_NO_DEVFILE_FOR_LAUNCH = "Could not find /dev/v3-vm# file to launch VM";
const char* ERROR_STOP_VM = "VM is not running";
const char* ERROR_VM_NOT_INVENTORY = "Cannot stop VM. VM is either inactive or not available in inventory";
const char* ERROR_LAUNCH_VM_DEVICE_NOT_FOUND = "Error launching VM! Device file not found";
const char* ERROR_LAUNCH_VM_IOCTL = "VM Launch: IOCTL error! Check kernel logs for details";
const char* ERROR_STOP_VM_PATH = "Error executing stop command. Check path variable";
const char* ERROR_STOP_VM_IOCTL = "VM Stop: IOCTL error! Check kernel logs for details";
const char* ERROR_STOP_VM_DEVICE_NOT_FOUND = "Could not stop VM! Device file not found";
const char* ERROR_PAUSE_VM_IOCTL = "VM Pause: IOCTL error! Check kernel logs for details";
const char* ERROR_PAUSE_VM_DEVICE_NOT_FOUND = "Error pausing VM! Could not open device file";
const char* ERROR_RESTART_VM_IOCTL = "VM Restart: IOCTL error! Check kernel logs for details";
const char* ERROR_RESTART_VM_DEVICE_NOT_FOUND = "Error restarting VM! Device file not found.";

const char* ERROR_VM_CREATE_PATH = "VM creation failed: Check PATH variable";
const char* ERROR_VM_CREATE_IOCTL = "VM creation failed: Check kernel logs for more details";
const char* ERROR_VM_CREATE_DB = "VM creation failed: Could not save VM. Error in database";
const char* ERROR_VM_CREATE_PROC = "VM creation failed: Unable to get dev file";
const char* ERROR_VM_CREATE_FAILED = "VM creation failed: Could not create dev file for new VM. Check kernel logs for more details";
const char* SUCCESS_VM_ADDED = "VM added to inventory. Activate to use";
const char* SUCCESS_VM_CREATE = "VM created successfully!";

const char* VM_TAB_TITLE = "VM Details";

const char* ERROR_VM_LAUNCH = "VM Launch failed: Check kernel logs for details";

const char* ERROR_VM_DELETE_PATH = "VM Deletion failed: Check PATH variable";
const char* ERROR_VM_DELETE_IOCTL = "VM Deletion failed: Check kernel logs for more details";
const char* ERROR_VM_DELETE_DB = "VM Deletion failed: Database error";
const char* ERROR_VM_DELETE_INVALID_ARGUMENT = "VM Deletion failed: Could not find dev file!";
const char* SUCCESS_VM_DELETE = "VM deleted successfully";
#endif /* DEFS_H_ */
