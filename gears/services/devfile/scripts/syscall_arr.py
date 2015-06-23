import bs4

#http://blog.rchapman.org/post/36801038863/linux-system-call-table-for-x86-64

def fd_arr():
	ff = open('syscalltable.txt','r')
	soup = bs4.BeautifulSoup(ff.read())
	ff.close()
	ff = open('sys_fd_arr.h','w')
	
	len_list = len(list(soup.find_all('tr')))
	ff.write('int sys_pointer_arr[323] = {\n')
	for (i,row) in enumerate(list(soup.find_all('tr'))):
		print str(row.find('td'))[4:len(str(row.find('td')))-5]
		
		if len(list(row.find_all('td')))>0:
			candidate = -1
			for (j,ele) in enumerate(list(row.find_all('td'))[2::]):
				inner = str(ele)[4:len(str(ele))-5]
				inner_split = inner.split()
				for inn in inner_split:
					if 'fd' in inn:
						print inner
						candidate = j
			if i>=len_list-1:
				ff.write(str(candidate)+'\n')
			else:
				ff.write(str(candidate)+',\n')
	ff.write('};\n')
	ff.close()
	return

def main():
	ff = open('syscalltable.txt','r')
	soup = bs4.BeautifulSoup(ff.read())
	ff.close()
	ff = open('sys_point_arr.h','w')
	
	len_list = len(list(soup.find_all('tr')))
	ff.write('int sys_pointer_arr[323] = {\n')
	for (i,row) in enumerate(list(soup.find_all('tr'))):
		print str(row.find('td'))[4:len(str(row.find('td')))-5]
		bit_vec = ''
		if len(list(row.find_all('td')))>0:
			for ele in list(row.find_all('td'))[2::]:
				inner = str(ele)[4:len(str(ele))-5]
				if '*' in inner:
					bit_vec+= '1'
				elif '' == inner:
					bit_vec+= '0'
				else:
					bit_vec+= '0'
			bit_vec+='00'
			print bit_vec[::-1]
			print int(bit_vec[::-1],2)
			if i>=len_list-1:
				ff.write(str(int(bit_vec[::-1],2))+'\n')
			else:
				ff.write(str(int(bit_vec[::-1],2))+',\n')
	ff.write('};\n')
	ff.close()
	return


if __name__ == "__main__":
    main()