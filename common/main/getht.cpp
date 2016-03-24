#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include "math.h"
#include "vecmat.h"
#include "getht.h"
vms_matrix getht(vms_matrix &trackm)
{	
	std::ifstream fifo;
	fifo.open("/tmp/fifo");
		if(! fifo.is_open()){
		std::cout << "error : cannot open file " << std :: endl;		
	}
	std::cout << "file open" << std :: endl;
	std::string line;
	bool done = false;
	while (!done)
		{
		while (std::getline(fifo, line))
		{
			/*std::cout << line << std::endl;*/
			float		heading;
			float		pitch;
			float		roll;
			float		xpos;
			float		ypos;
			float		zpos;
			float		counter;

			std::stringstream data(line);
			data >> heading >> pitch >> roll >> xpos >> ypos >> zpos >> counter;
			
			fix fpitch = fl2f(pitch);
			fix fheading = fl2f(heading);
			vms_angvec headOffset;
			headOffset.h = headOffset.p = headOffset.b = 0;
			headOffset.h += -fheading/100;
			headOffset.p += -fpitch/100;
			vms_matrix vecmat = vm_angles_2_matrix(headOffset);
			trackm = vm_matrix_x_matrix(vecmat, trackm);
			return trackm;
			
		}
		if (fifo.eof())
		{
			fifo.clear();
		}
		else
		{
			done = true;
		}
	
	}
	
};
