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
			
			fix fixx = fl2f(xpos);
			fix fixy = fl2f(ypos);
			fix fixz = fl2f(zpos);
			vms_vector vector;
			vector.x=fixx;
			vector.y=fixy;
			vector.z=fixz;
			vms_matrix vecmat;  
			vecmat = vm_vector_2_matrix(vector,&vector,&vector);
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
