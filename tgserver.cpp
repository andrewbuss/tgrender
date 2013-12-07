#include <GL/glew.h>
#include <GL/glu.h>
#include <fstream>
#include <math.h>
#include <iostream>
#include <string.h>
#include <stdlib.h>
#include "Poco/Net/ServerSocket.h"
#include "Poco/Net/TCPServer.h"
#include "Poco/Net/TCPServerConnectionFactory.h"

using namespace std;
using namespace Poco::Net;

struct frameheader{
  char info[32];
  unsigned int framenumber;
  float frametime;
  float cx;
  float cy;
  float cz;
  float lx;
  float ly;
  float lz;
  float pointsize;
  unsigned int xres;
  unsigned int yres;
  unsigned int mapheight;
  unsigned int mapwidth;
};

class RendererConnection:public TCPServerConnection{
public:
  RendererConnection(const StreamSocket& s): TCPServerConnection(s){}
  void run(){
    StreamSocket& ss = socket();
    char* buffer = (char*) malloc(1048576);
    cout << "Received connection\n";
    int receivedbytes = ss.receiveBytes(buffer, sizeof(frameheader));
    frameheader header; 
    memcpy(&header, buffer, sizeof(frameheader));
    cout << "Frame header size: " << sizeof(frameheader) << '\n';
    cout << "Frame info  : " << header.info << '\n';
    cout << "Frame number: " << header.framenumber << '\n';
    cout << "Frame time  : " << header.frametime << '\n';
    cout << "Camera coord: " << header.cx << ", " << header.cy << ", " << header.cz << '\n';
    cout << "Lookat coord: " << header.lx << ", " << header.ly << ", " << header.lz << '\n';
    cout << "Point size  : " << header.pointsize << '\n';
    cout << "Resolution  : " << header.xres << " x " << header.yres << '\n';
    cout << "Map size    : " << header.mapwidth << " x " << header.mapheight << '\n';
    int npoints = header.mapwidth*header.mapheight;
    float* points = (float*) malloc(npoints*12);
    unsigned char* result = (unsigned char*) malloc(4*header.xres*header.yres);
    unsigned char* colors = (unsigned char*) malloc(npoints*4);
    cout << "Receiving point data...\n";
    int offset = 0;
    while(offset<npoints){
      receivedbytes = 0;
      while(receivedbytes<4) receivedbytes += ss.receiveBytes(colors+offset*4+receivedbytes,4-receivedbytes);
      receivedbytes = 0;
      while(receivedbytes<12) receivedbytes += ss.receiveBytes(((char*) points)+offset*12+receivedbytes,12-receivedbytes);
      offset++;
    }
    cout << "Received " << npoints << " points\n";
    cout << "Rendering frame\n";
    GLuint buffernames[3];
    glGenBuffers(2,buffernames);
    glGenFrameBuffers(1,&buffernames[2]);
    glBindFrameBuffer(GL_FRAMEBUFFER, buffernames[2]);
    glViewport(0, 0, header.xres, header.yres);    
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glClearColor(0, 0, 0, 0);
    glClearDepth(1.0f);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0f, header.xres/(float)header.yres, 0.001f, 500.0f);
    glEnable(GL_POINT_SMOOTH);
    glEnable(GL_BLEND);
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    glMatrixMode(GL_MODELVIEW);
    glEnable(GL_MULTISAMPLE);
    glBindBuffer(GL_ARRAY_BUFFER, buffernames[0]);
    glBufferData(GL_ARRAY_BUFFER, npoints*4, colors, GL_STATIC_DRAW);
    glColorPointer(4, GL_UNSIGNED_BYTE, 0, 0);
    glBindBuffer(GL_ARRAY_BUFFER, buffernames[1]);
    glBufferData(GL_ARRAY_BUFFER, npoints*12, points, GL_STATIC_DRAW);
    glVertexPointer(3, GL_FLOAT, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();
    gluLookAt(header.cx,header.cy, header.cz,header.lx,header.ly,header.lz,0,1,0);
    glPointSize(header.pointsize);
    glDrawArrays(GL_POINTS, 0, npoints);
    cout << "Saving frame\n";
    glReadPixels(0,0,header.xres,header.yres,GL_RGBA,GL_UNSIGNED_INT_8_8_8_8_REV, result);
    unsigned char* temp = (unsigned char*)malloc(header.xres*4);
    for(int y=0; y<header.yres/2; y++){
      for(int x=0; x<header.xres*4; x++){
	temp[x]=result[header.xres*(header.yres-y-1)*4+x];
	result[header.xres*(header.yres-y-1)*4+x]=result[header.xres*y*4+x];
	result[header.xres*y*4+x] =temp[x];
      }
    }
    free(temp);
    int totalsent=0;
    cout << "Sending frame\n";
    while(totalsent<4*header.xres*header.yres){
      totalsent+=ss.sendBytes(result+totalsent,4096);
    }
    cout << "Sent frame, " << totalsent << " bytes, shutting this thread down\n";
    free(buffer);
    free(points);
    free(colors);
    free(result);
  }
};

int main(){
  ServerSocket socket(7676);
  TCPServer server(new TCPServerConnectionFactoryImpl<RendererConnection>(),socket);
  server.start();
  cout << "Awaiting connections on port 7676\n";
  cin.ignore();
  return 0;
}

