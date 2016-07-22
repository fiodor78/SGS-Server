/***************************************************************************
 ***************************************************************************
  (c) 1999-2006 Ganymede Technologies                 All Rights Reserved
      Krakow, Poland                                  www.ganymede.eu
 ***************************************************************************
 ***************************************************************************/

#include "headers_all.h"

/***************************************************************************/
GMainConsole::GMainConsole()
{

}
/***************************************************************************/
bool GMainConsole::Init()
{
	FILE * consolefile  = fopen("socket","r+");
	port=0;
	char host[128];
	if (consolefile != NULL)
	{
		fscanf(consolefile,"%127[^:]:%d",host,&port);
		fclose(consolefile);
	}
	if(port==0)
	{
		cout<<"connection error";
		return false;
	}


	client_socket=socket(AF_INET, SOCK_STREAM,0);
	struct	sockaddr_in	server_address;
	server_address.sin_family=AF_INET;
	server_address.sin_port=htons((USHORT16)port);
	server_address.sin_addr.s_addr=GetAddrFromName(host);
	memset(&server_address.sin_zero,0,8);

	int ret=connect(client_socket,(struct sockaddr *)&server_address,sizeof(server_address));
	ret=GET_SOCKET_ERROR();
	if (ret==EWOULDBLOCK || ret==0)
	{
	}
	else
	{
		cout<<"connection error";
		return false;
	}
	return true;
}
/***************************************************************************/
bool GMainConsole::Action(string s)
{
	char buf[256];
	//sprintf(buf,"autoclose %s",s.c_str());
	sprintf(buf,"%s",s.c_str());
	int len=(int)strlen(buf);
	int size=send(client_socket,buf,len,0);
	if ((size==SOCKET_ERROR) || (size==0) || (size!=len))
	{
		cout<<"communication error";
	}
	else
	{
		int size=1;
		char buf[1024];
		while(size)
		{
			size=recv(client_socket,buf,1023,0);
			buf[size]=0;
			cout<<buf;
		}
		return true;
	}
	return true;
}
/***************************************************************************/
bool GMainConsole::Destroy()
{
	closesocket(client_socket);
	return true;
}
/***************************************************************************/

