

def main():
	ff = open("guest_devices.txt",'r')
	
	count = 0
	out = ''
	for line in ff:
		count+=1
		this_line = '{\"'+line.strip('\n')+'\",-1},\n'
		out += this_line
	out = out.rstrip('\n,')
	out+="\n"
	top = "#define DEV_COUNT "+str(count)+"\n"
	
	ff.close()
	
	writeable = open("dev_file_guest_fd_tracker.h",'r');
	lst = writeable.readlines()
	writeable.close()
	count = 0
	for line in lst:
		count+=1
		if 'PYTHONSCRIPTBREAK1' in line:
			index1 = count
		if 'PYTHONSCRIPTBREAK3' in line:
			index2 = count
		if 'PYTHONSCRIPTBREAK4' in line:
			index3 = count
	
	lst.pop(index1)
	lst.insert(index1,top)
	subcount = index3-2
	while(subcount>= index2):
		lst.pop(subcount)
		subcount-=1
	lst.insert(index2,out)
	writeable = open("dev_file_guest_fd_tracker.h",'w');
	writeable.write(''.join(lst))
	writeable.close()
	return
	

if __name__ == "__main__":
    main()