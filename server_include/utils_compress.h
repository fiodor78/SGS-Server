/***************************************************************************
 ***************************************************************************
  (c) 1999-2006 Ganymede Technologies                 All Rights Reserved
      Krakow, Poland                                  www.ganymede.eu
 ***************************************************************************
 ***************************************************************************/

#include "zlib.h"

/***************************************************************************/
class Compress: boost::noncopyable
{
public:
	Compress(const std::string p,std::string ext)
	{
		try{
		boost::filesystem::path path(p,boost::filesystem::native);
		if(boost::filesystem::exists(path))
		{
			int size_src=(int)boost::filesystem::file_size(path);
			FILE * f=fopen(p.c_str(),"rb");
			if(f)
			{
				Wrap<BYTE> buf_src=new BYTE[size_src];
				fread(buf_src,size_src,1,f);
				fclose(f);

				unsigned long size_dest=size_src*2;
				Wrap<BYTE> buf_dest=new BYTE[size_dest];
				if(!compress(buf_dest,&size_dest,buf_src,size_src))
				{
					std::string p_gz=p+ext;
					FILE * f_gz=fopen(p_gz.c_str(),"wb");
					if(f_gz)
					{
						fwrite(buf_dest,size_dest,1,f_gz);
						fclose(f_gz);
						dest=p_gz;
					}
				}
			}
		}
		}catch(...){};
	}
	std::string operator()(){return dest;};
	std::string dest;
};
/***************************************************************************/
