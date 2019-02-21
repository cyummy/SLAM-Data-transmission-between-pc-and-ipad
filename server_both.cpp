//作为接收端想显示图片
//ios传来数据是：先传来深度图，16位;再传来rgb图
//深度图用libpng库，rgb图用libjpeg库
#include <stdio.h>
#include <iostream>
#include <stdlib.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <zlib.h>

#include <opencv2/opencv.hpp>

//libpng to decompress depth image
//libjpeg to decompress rgb image
extern "C"
{
#include "png.h"
#include "jpeglib.h"
}

#define PORT 10001
#define PNG_BYTES_TO_CHECK 4

using namespace std;
using namespace cv;

typedef struct {
    uint8_t* data;
    int size;
    int offset;
}ImageSource;

//-----------------------------------------------------------------------------------
static void pngReaderCallback(png_structp png_ptr, png_bytep data, png_size_t length)
{
    ImageSource* isource = (ImageSource*)png_get_io_ptr(png_ptr);

    if(isource->offset + length <= isource->size)
    {
      memcpy(data,isource->data + isource->offset,  length);
      isource->offset += length;
    }
    //else
    //{
     //   png_error(png_ptr,"pngReaderCallback failed");
    //}
}

//将功能整合，想法是接收数据方式不变，只是改变一下解析数据的方式
//由于先传来的是深度图，再是彩色图，故根据计数值，偶数就解析深度图，奇数就解析rgb图
int main()
{
    uint8_t imgbuf[120000]={0};//发送端传来的数据先全部放在这儿
    uint8_t real_imbuf[640*480*3];//image buffer
    uint8_t color_imgbuf[640*480*3];//真正的rgb图放在这儿
    uint16_t depth_imgbuff[640*480];//真正的深度图放在这儿
                             
    const size_t recv_length=1024;
    const int int_len=sizeof(int);
    uint8_t rec_buff[recv_length];//recv buffer of each time
    uint8_t next_buff[recv_length];//to store the part of next image
    uint8_t next_buff_length[int_len];//to store the length of next image
    bool next_img_flag=false;
    bool next_length_flag=false;
    bool next_length_part_flag=false;
    int next_length_part_len;
    int next_part_len;
    
    //-----set up socket
    struct sockaddr_in s_in;//server address structure
    struct sockaddr_in c_in;//client address structure
    int l_fd,c_fd;
    
    socklen_t len;
    memset((void *)&s_in,0,sizeof(s_in));
    s_in.sin_family = AF_INET;//IPV4 communication domain
    s_in.sin_addr.s_addr = INADDR_ANY;//accept any address
    s_in.sin_port = htons(PORT);//change port to netchar
 
    l_fd = socket(AF_INET,SOCK_STREAM,0);//socket(int domain, int type, int protocol)
    bind(l_fd,(struct sockaddr *)&s_in,sizeof(s_in));
    listen(l_fd,1);//同时只能有一个连接

    socklen_t sin_size=sizeof(struct sockaddr_in);
    c_fd = accept(l_fd,(struct sockaddr *)&c_in,&sin_size);
    //-----set up socket
    
    //---------libjpeg decompression----------
    //存储了一些和解压缩jpeg图片相关的配置
    struct jpeg_decompress_struct cinfo;  
    struct jpeg_error_mgr jerr;  
    
    cinfo.err = jpeg_std_error(&jerr);
    
    Mat color_image;
    int width;
    int height;
    
    //---------libpng decompression----------
    png_structp png_ptr;
    png_infop info_ptr;
    png_bytep* row_pointers;
    char buf[PNG_BYTES_TO_CHECK];
    int w, h, d, x, y, temp, color_type;
//  Mat depth_image_16 = Mat(240,320,CV_16UC1);
    Mat depth_image_16;

//  png_ptr = png_create_read_struct( PNG_LIBPNG_VER_STRING, 0, 0, 0 );
//  info_ptr = png_create_info_struct( png_ptr );

    ImageSource imgsource;
    
    //-------loop control variable----------
    int mysum=0;
    int n;
    int img_counter=0;
    int img_length;
    int next_im_length;
    int *p_im_length;
    
    Mat img_depth;
//  int width ;
//  int height ;

    //----------while--------
    cout<<"begin............"<<endl<<endl;   
    while(img_counter<2000){ //*******while 2******
        cout<<"img_counter:"<<img_counter<<"--->"<<(img_counter%2?"color":"depth")<<endl;
        //here,first get the length of the image
        //----size of recv buffer
        if(!next_length_flag){//未获得下一张图片的长度
	        if(next_length_part_flag){
	            recv(c_fd,next_buff_length+(int_len-next_length_part_len),next_length_part_len,0);
	            p_im_length=(int*)next_buff_length;
	            img_length=*p_im_length;
	            cout<<"++++++a new pic++++++++"<<endl;
	            cout<<"image buffer size:"<<img_length<<endl;
	            next_length_part_flag=false;
	        }
	        else{
	            recv(c_fd,&img_length,sizeof(int),0);
	            cout<<"++++++a new pic++++++++"<<endl;
	            cout<<"image buffer size:"<<img_length<<endl;
	        }
        }  
      
        next_length_flag=false;
      
        while(mysum<img_length){//*******while 1******
	        n = recv(c_fd,rec_buff,recv_length,0);//read the message sent by client
	  
	        //mysum+n is the length of buffer U have read
	        if(mysum+n<=img_length){//++++++++++++小于或者等于图像边界
	        	memcpy(imgbuf+mysum,rec_buff,n);
	        	mysum+=n;
	    	}else 
	    	if(mysum+n<img_length+int_len){//++++++++++++大于图像边界，但图像长度未包含完全
	        	cout<<"if 22222"<<endl;
	    
				memcpy(imgbuf+mysum,rec_buff,img_length-mysum);//so,here a full image is ok
				memcpy(next_buff_length,rec_buff+img_length-mysum,mysum+n-img_length);//here can't get length of next image
			
				next_length_part_flag=true;//获得下一张图片长度的一部分数据
				next_length_part_len=int_len-(mysum+n-img_length);//剩余的未读完的长度的字节数
				mysum=img_length;
			}else 
			if(mysum+n==img_length+int_len){//++++++++++++大于图像边界，恰好包含图像长度
				cout<<"if 33333"<<endl;
			
				memcpy(imgbuf+mysum,rec_buff,img_length-mysum);//so,here a full image is ok
				memcpy(&next_im_length,rec_buff+img_length-mysum,int_len);//here get length of next image
			
	// 	    p_im_length=(int*)next_buff_length;
			
				next_length_flag=true;//获得了下一张图片的长度
				mysum=img_length;
			}
			else{//most frequentlly condition;大于图像边界，包含图像长度，包含下一张图片的部分
				cout<<endl<<"if 44444"<<endl;
			
				memcpy(imgbuf+mysum,rec_buff,img_length-mysum);//so,here a full image is ok
				memcpy(&next_im_length,rec_buff+img_length-mysum,int_len);//here get length of next image
			
	// 	    p_im_length=(int*)next_buff_length;
			
				next_part_len=mysum+n-img_length-int_len;//表示获得的下一张图片部分的长度
				memcpy(next_buff,rec_buff+img_length-mysum+int_len,next_part_len);
			
				next_length_flag=true;//获得了下一张图片的长度
				next_img_flag=true;//此外，还获得了下一张图片的部分数据
				mysum=img_length;
			} 
        }//*******while 1******      
        cout<<endl<<"sum: "<<mysum<<endl<<endl;   
      

        if(img_counter%2==0)//偶数次为深度图
        {
			png_ptr = png_create_read_struct( PNG_LIBPNG_VER_STRING, 0, 0, 0 );
			info_ptr = png_create_info_struct( png_ptr );
    
			imgsource.data = (uint8_t*)imgbuf;
			imgsource.size = img_length;
			imgsource.offset = 0;
			//define our own callback function for I/O instead of reading from a file
			png_set_read_fn(png_ptr,&imgsource, pngReaderCallback);
			
			setjmp( png_jmpbuf(png_ptr) ); 
			
			memcpy(buf,imgbuf,PNG_BYTES_TO_CHECK);  
			//cout<<endl<<buf<<endl;
			/* 检测数据是否为PNG的签名 */
			temp = png_sig_cmp( (png_bytep)buf, (png_size_t)0, PNG_BYTES_TO_CHECK );
			//cout<<temp<<endl;
			/* 如果不是PNG的签名，则说明该文件不是PNG文件 */
			if( temp != 0 ) 
			{
				png_destroy_read_struct( &png_ptr, &info_ptr, 0);
				return -1;
			}
			
			png_set_sig_bytes(png_ptr, 0);
			
			/* 读取PNG图片信息和像素数据 */
			png_read_png( png_ptr, info_ptr, PNG_TRANSFORM_EXPAND, 0 );
			//png_get_IHDR(png_ptr,info_ptr,&w,&h,&d,&color_type,NULL,NULL,NULL);
			/* 获取图像的色彩类型 */
			color_type = png_get_color_type( png_ptr, info_ptr );
			/* 获取图像的宽高 */
			w = png_get_image_width( png_ptr, info_ptr );
			h = png_get_image_height( png_ptr, info_ptr );
			d = png_get_bit_depth( png_ptr, info_ptr);
			cout<<"width :"<<w<<"  height:"<<h<<endl; 
			
			/* 获取图像的所有行像素数据*/
			//png_set_rows(png_ptr, info_ptr, &row_pointers);
			row_pointers = png_get_rows( png_ptr, info_ptr);
			/* 根据不同的色彩类型进行相应处理 */
			switch( color_type ) {
			case PNG_COLOR_TYPE_GRAY:
				for( y=0; y<h; ++y ) {
					for( x=0; x<w*d/8; ) { 
						//buf[y*w*2+x] = row_pointers[y][x++]; 
						int high,low,pix;
						pix = x/2;
						high= row_pointers[y][x++];
						low= row_pointers[y][x++];
		// 			      depth_image_16.at<uint16_t>(y,pix) = high*256+low;    
						depth_imgbuff[y*w+pix]=high*256+low;
						
					}
				}
				break;
			/* 其它色彩类型的图像就不读了 */
			default:
				png_destroy_read_struct( &png_ptr, &info_ptr, 0);
				return -1;
			}
			png_destroy_read_struct( &png_ptr, &info_ptr, 0);
			
			depth_image_16=Mat(480,640,CV_16UC1,depth_imgbuff);//depth_imgbuff
			}//---------libpng decompression----------
        else //---------libjpeg decompression----------
      	{
	 		jpeg_create_decompress(&cinfo); 
		
			jpeg_mem_src(&cinfo,imgbuf,mysum);
		
			jpeg_read_header(&cinfo, TRUE); 
		
			jpeg_start_decompress(&cinfo);
		
			width = cinfo.output_width;
			height = cinfo.output_height;
			cout<<"width :"<<width<<"  height:"<<height<<endl; 
			//---------libjpeg decompression----------
		
			JSAMPARRAY buffer = (*cinfo.mem->alloc_sarray)((j_common_ptr)&cinfo, JPOOL_IMAGE, width * 3, 1);

			for(int h_counter=0;h_counter<height;h_counter++)
			{
				jpeg_read_scanlines(&cinfo, buffer, 1);
			
				unsigned char * bgr = (unsigned char *)buffer[0];
			
				for(int i = 0; i < width; i++)
				{
					color_imgbuf[h_counter*width*3+i*3+0]=bgr[i*3+2];
					color_imgbuf[h_counter*width*3+i*3+1]=bgr[i*3+1];
					color_imgbuf[h_counter*width*3+i*3+2]=bgr[i*3+0]; 
				}
			}
		
			jpeg_finish_decompress(&cinfo);
			jpeg_destroy_decompress(&cinfo);
		
			color_image=Mat(480,640,CV_8UC3,color_imgbuf);//color_imgbuf
    	}
        //---------libjpeg decompression----------
      
    //----------opencv show image---------
    //两张图片一起显示，所以得接收了一对图片才行
    //故计数次数应为单数次数
    if(img_counter%2==1){
		imshow("Depth Image",depth_image_16*2);
		imshow("Color Image",color_image);
		
		waitKey(1);
    }
     
    if(img_counter==1)
    {
       imwrite("../depth.png",depth_image_16);
       imwrite("../color.jpg",color_image);
    }
     
    cout<<"----------a picture is ok-------------"<<endl<<endl; 

      //为下一次的循环做准备
    memset(imgbuf,0,sizeof(imgbuf));
      
    if(next_img_flag){//此flag表示已经读取到下一张图片的一部分，需要将这部分保存到imbuf中
	    memcpy(imgbuf,next_buff,next_part_len);
	    mysum=next_part_len;
	    next_img_flag=false;
    }
    else{
	    mysum=0;
    }
      
    if(next_length_flag){
	    img_length=next_im_length;
	    cout<<"++++++a new pic++++++++"<<endl;
	    cout<<"image buffer size:"<<img_length<<endl;
    }
      
    img_counter++;//image number counter
}//*******while 2******

    close(c_fd);
    close(l_fd);
    
    return 0;
}
