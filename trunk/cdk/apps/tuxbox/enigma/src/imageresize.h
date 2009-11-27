#ifndef __imageresize_h__
#define __imageresize_h__

#define asm __asm

typedef unsigned char TUInt8; // [0..255]
struct TARGB32 //32 bit color
{
	TUInt8 b,g,r,a; //a is alpha
};

struct TPicRegion //一块颜色数据区的描述，便于参数传递
{
	TARGB32* pdata; //颜色数据首地址
	long byte_width; //一行数据的物理宽度(字节宽度)；
	//abs(byte_width)有可能大于等于width*sizeof(TARGB32);
	long width; //像素宽度
	long height; //像素高度
};

//那么访问一个点的函数可以写为：
inline TARGB32& Pixels(const TPicRegion& pic,const long x,const long y)
{
	return ( (TARGB32*)((TUInt8*)pic.pdata+pic.byte_width*y) )[x];
}

//访问一个点的函数，(x,y)坐标可能超出图片边界,进行边界饱和
inline TARGB32& Pixels_Bound(const TPicRegion& pic,long x,long y)
{
	//assert((pic.width>0)&&(pic.height>0));
	if (x<0) {x=0; } else if (x>=pic.width ) {x=pic.width -1; }
	if (y<0) {y=0; } else if (y>=pic.height) {y=pic.height-1; }
	return Pixels(pic,x,y);
}

/////////////////////////////////////////////////////////////////////
////近邻取样插值
////////////////////////////////////////////////////////////////////
void PicZoom3(const TPicRegion& Dst,const TPicRegion& Src)
{
    if (  (0==Dst.width)||(0==Dst.height)
        ||(0==Src.width)||(0==Src.height)) return;
    unsigned long xrIntFloat_16=(Src.width<<16)/Dst.width+1;
    unsigned long yrIntFloat_16=(Src.height<<16)/Dst.height+1;
    long dst_width=Dst.width;
    TARGB32* pDstLine=Dst.pdata;
    unsigned long srcy_16=0;
    for (long y=0;y<Dst.height;++y)
    {
        TARGB32* pSrcLine=((TARGB32*)((TUInt8*)Src.pdata+Src.byte_width*(srcy_16>>16)));
        unsigned long srcx_16=0;
        for (long x=0;x<dst_width;++x)
        {
            pDstLine[x]=pSrcLine[srcx_16>>16];
            srcx_16+=xrIntFloat_16;
        }
        srcy_16+=yrIntFloat_16;
        ((TUInt8*&)pDstLine)+=Dst.byte_width;
    }
}

//////////////////////////////////////////////////////////////////////////
//二次线性插值缩放算法
///////////////////////////////////////////////////////////////////////////

    inline void Bilinear_Fast_Common(TARGB32* PColor0,TARGB32* PColor1,unsigned long u_8,unsigned long v_8,TARGB32* result)
    {
        unsigned long pm3_8=(u_8*v_8)>>8;
        unsigned long pm2_8=u_8-pm3_8;
        unsigned long pm1_8=v_8-pm3_8;
        unsigned long pm0_8=256-pm1_8-pm2_8-pm3_8;

        unsigned long Color=*(unsigned long*)(PColor0);
        unsigned long BR=(Color & 0x00FF00FF)*pm0_8;
        unsigned long GA=((Color & 0xFF00FF00)>>8)*pm0_8;
                      Color=((unsigned long*)(PColor0))[1];
                      GA+=((Color & 0xFF00FF00)>>8)*pm2_8;
                      BR+=(Color & 0x00FF00FF)*pm2_8;
                      Color=*(unsigned long*)(PColor1);
                      GA+=((Color & 0xFF00FF00)>>8)*pm1_8;
                      BR+=(Color & 0x00FF00FF)*pm1_8;
                      Color=((unsigned long*)(PColor1))[1];
                      GA+=((Color & 0xFF00FF00)>>8)*pm3_8;
                      BR+=(Color & 0x00FF00FF)*pm3_8;

        *(unsigned long*)(result)=(GA & 0xFF00FF00)|((BR & 0xFF00FF00)>>8);
    }

    inline void Bilinear_Border_Common(const TPicRegion& pic,const long x_16,const long y_16,TARGB32* result)
    {
        long x=(x_16>>16);
        long y=(y_16>>16);
        unsigned long u_16=((unsigned short)(x_16));
        unsigned long v_16=((unsigned short)(y_16));

        TARGB32 pixel[4];
        pixel[0]=Pixels_Bound(pic,x,y);
        pixel[1]=Pixels_Bound(pic,x+1,y);
        pixel[2]=Pixels_Bound(pic,x,y+1);
        pixel[3]=Pixels_Bound(pic,x+1,y+1);
        
        Bilinear_Fast_Common(&pixel[0],&pixel[2],u_16>>8,v_16>>8,result);
    }

void PicZoom_Bilinear_Common(const TPicRegion& Dst,const TPicRegion& Src)
{
    if (  (0==Dst.width)||(0==Dst.height)
        ||(0==Src.width)||(0==Src.height)) return;

    long xrIntFloat_16=((Src.width)<<16)/Dst.width+1; 
    long yrIntFloat_16=((Src.height)<<16)/Dst.height+1;
    const long csDErrorX=-(1<<15)+(xrIntFloat_16>>1);
    const long csDErrorY=-(1<<15)+(yrIntFloat_16>>1);

    long dst_width=Dst.width;

    //计算出需要特殊处理的边界
    long border_y0=-csDErrorY/yrIntFloat_16+1;              //y0+y*yr>=0; y0=csDErrorY => y>=-csDErrorY/yr
    if (border_y0>=Dst.height) border_y0=Dst.height;
    long border_x0=-csDErrorX/xrIntFloat_16+1;     
    if (border_x0>=Dst.width ) border_x0=Dst.width; 
    long border_y1=(((Src.height-2)<<16)-csDErrorY)/yrIntFloat_16+1; //y0+y*yr<=(height-2) => y<=(height-2-csDErrorY)/yr
    if (border_y1<border_y0) border_y1=border_y0;
    long border_x1=(((Src.width-2)<<16)-csDErrorX)/xrIntFloat_16+1; 
    if (border_x1<border_x0) border_x1=border_x0;

    TARGB32* pDstLine=Dst.pdata;
    long Src_byte_width=Src.byte_width;
    long srcy_16=csDErrorY;
    long y;
    for (y=0;y<border_y0;++y)
    {
        long srcx_16=csDErrorX;
        for (long x=0;x<dst_width;++x)
        {
            Bilinear_Border_Common(Src,srcx_16,srcy_16,&pDstLine[x]); //border
            srcx_16+=xrIntFloat_16;
        }
        srcy_16+=yrIntFloat_16;
        ((TUInt8*&)pDstLine)+=Dst.byte_width;
    }
    for (y=border_y0;y<border_y1;++y)
    {
        long srcx_16=csDErrorX;
        long x;
        for (x=0;x<border_x0;++x)
        {
            Bilinear_Border_Common(Src,srcx_16,srcy_16,&pDstLine[x]);//border
            srcx_16+=xrIntFloat_16;
        }

        {
            unsigned long v_8=(srcy_16 & 0xFFFF)>>8;
            TARGB32* PSrcLineColor= (TARGB32*)((TUInt8*)(Src.pdata)+Src_byte_width*(srcy_16>>16)) ;
            for (long x=border_x0;x<border_x1;++x)
            {
                TARGB32* PColor0=&PSrcLineColor[srcx_16>>16];
                TARGB32* PColor1=(TARGB32*)((TUInt8*)(PColor0)+Src_byte_width);
                Bilinear_Fast_Common(PColor0,PColor1,(srcx_16 & 0xFFFF)>>8,v_8,&pDstLine[x]);
                srcx_16+=xrIntFloat_16;
            }
        }

        for (x=border_x1;x<dst_width;++x)
        {
            Bilinear_Border_Common(Src,srcx_16,srcy_16,&pDstLine[x]);//border
            srcx_16+=xrIntFloat_16;
        }
        srcy_16+=yrIntFloat_16;
        ((TUInt8*&)pDstLine)+=Dst.byte_width;
    }
    for (y=border_y1;y<Dst.height;++y)
    {
        long srcx_16=csDErrorX;
        for (long x=0;x<dst_width;++x)
        {
            Bilinear_Border_Common(Src,srcx_16,srcy_16,&pDstLine[x]); //border
            srcx_16+=xrIntFloat_16;
        }
        srcy_16+=yrIntFloat_16;
        ((TUInt8*&)pDstLine)+=Dst.byte_width;
    }
}

//////////////////////////////////////////////
//三次卷积插值
////////////////////////////////////////////

    inline double SinXDivX(double x) 
    {
        //该函数计算插值曲线sin(x*PI)/(x*PI)的值 //PI=3.1415926535897932385; 
        //下面是它的近似拟合表达式
        const float a = -1; //a还可以取 a=-2,-1,-0.75,-0.5等等，起到调节锐化或模糊程度的作用

        if (x<0) x=-x; //x=abs(x);
        double x2=x*x;
        double x3=x2*x;
        if (x<=1)
          return (a+2)*x3 - (a+3)*x2 + 1;
        else if (x<=2) 
          return a*x3 - (5*a)*x2 + (8*a)*x - (4*a);
        else
          return 0;
    } 

    static long SinXDivX_Table_8[(2<<8)+1];
    class _CAutoInti_SinXDivX_Table {
    private: 
        void _Inti_SinXDivX_Table()
        {
            for (long i=0;i<=(2<<8);++i)
                SinXDivX_Table_8[i]=long(0.5+256*SinXDivX(i*(1.0/(256))))*1;
        };
    public:
        _CAutoInti_SinXDivX_Table() { _Inti_SinXDivX_Table(); }
    };
    static _CAutoInti_SinXDivX_Table __tmp_CAutoInti_SinXDivX_Table;

    inline TUInt8 border_color(long Color)
    {
            if (Color<=0)
                return 0;
            else if (Color>=255)
                return 255;
            else
                return Color;
    }

    //颜色查表
    static TUInt8 _color_table[256*3];
    static const TUInt8* color_table=&_color_table[256];
    class _CAuto_inti_color_table
    {
    public:
        _CAuto_inti_color_table() {
            for (int i=0;i<256*3;++i)
                _color_table[i]=border_color(i-256);
        }
    };
    static _CAuto_inti_color_table _Auto_inti_color_table;

    void ThreeOrder_Fast_Common(const TPicRegion& pic,const long x_16,const long y_16,TARGB32* result)
    {
        unsigned long u_8=(unsigned char)((x_16)>>8);
        unsigned long v_8=(unsigned char)((y_16)>>8);
        const TARGB32* pixel=&Pixels(pic,(x_16>>16)-1,(y_16>>16)-1);
        long pic_byte_width=pic.byte_width;

        long au_8[4],av_8[4];
        //
        au_8[0]=SinXDivX_Table_8[(1<<8)+u_8];
        au_8[1]=SinXDivX_Table_8[u_8];
        au_8[2]=SinXDivX_Table_8[(1<<8)-u_8];
        au_8[3]=SinXDivX_Table_8[(2<<8)-u_8];
        av_8[0]=SinXDivX_Table_8[(1<<8)+v_8];
        av_8[1]=SinXDivX_Table_8[v_8];
        av_8[2]=SinXDivX_Table_8[(1<<8)-v_8];
        av_8[3]=SinXDivX_Table_8[(2<<8)-v_8];

        long sR=0,sG=0,sB=0,sA=0;
        for (long i=0;i<4;++i)
        {
            long aA=au_8[0]*pixel[0].a + au_8[1]*pixel[1].a + au_8[2]*pixel[2].a + au_8[3]*pixel[3].a;
            long aR=au_8[0]*pixel[0].r + au_8[1]*pixel[1].r + au_8[2]*pixel[2].r + au_8[3]*pixel[3].r;
            long aG=au_8[0]*pixel[0].g + au_8[1]*pixel[1].g + au_8[2]*pixel[2].g + au_8[3]*pixel[3].g;
            long aB=au_8[0]*pixel[0].b + au_8[1]*pixel[1].b + au_8[2]*pixel[2].b + au_8[3]*pixel[3].b;
            sA+=aA*av_8[i];
            sR+=aR*av_8[i];
            sG+=aG*av_8[i];
            sB+=aB*av_8[i];
            ((TUInt8*&)pixel)+=pic_byte_width;
        }

        result->a=color_table[sA>>16];
        result->r=color_table[sR>>16];
        result->g=color_table[sG>>16];
        result->b=color_table[sB>>16];
    }

    void ThreeOrder_Border_Common(const TPicRegion& pic,const long x_16,const long y_16,TARGB32* result)
    {
        long x0_sub1=(x_16>>16)-1;
        long y0_sub1=(y_16>>16)-1;
        unsigned long u_16_add1=((unsigned short)(x_16))+(1<<16);
        unsigned long v_16_add1=((unsigned short)(y_16))+(1<<16);

        TARGB32 pixel[16];
        long i;

        for (i=0;i<4;++i)
        {
            long y=y0_sub1+i;
            pixel[i*4+0]=Pixels_Bound(pic,x0_sub1+0,y);
            pixel[i*4+1]=Pixels_Bound(pic,x0_sub1+1,y);
            pixel[i*4+2]=Pixels_Bound(pic,x0_sub1+2,y);
            pixel[i*4+3]=Pixels_Bound(pic,x0_sub1+3,y);
        }
        
        TPicRegion npic;
        npic.pdata     =&pixel[0];
        npic.byte_width=4*sizeof(TARGB32);
        //npic.width     =4;
        //npic.height    =4;
        ThreeOrder_Fast_Common(npic,u_16_add1,v_16_add1,result);
    }

void PicZoom_ThreeOrder_Common(const TPicRegion& Dst,const TPicRegion& Src)
{
    if (  (0==Dst.width)||(0==Dst.height)
        ||(0==Src.width)||(0==Src.height)) return;

    long xrIntFloat_16=((Src.width)<<16)/Dst.width+1; 
    long yrIntFloat_16=((Src.height)<<16)/Dst.height+1;
    const long csDErrorX=-(1<<15)+(xrIntFloat_16>>1);
    const long csDErrorY=-(1<<15)+(yrIntFloat_16>>1);

    long dst_width=Dst.width;

    //计算出需要特殊处理的边界
    long border_y0=((1<<16)-csDErrorY)/yrIntFloat_16+1;//y0+y*yr>=1; y0=csDErrorY => y>=(1-csDErrorY)/yr
    if (border_y0>=Dst.height) border_y0=Dst.height;
    long border_x0=((1<<16)-csDErrorX)/xrIntFloat_16+1;
    if (border_x0>=Dst.width ) border_x0=Dst.width;
    long border_y1=(((Src.height-3)<<16)-csDErrorY)/yrIntFloat_16+1; //y0+y*yr<=(height-3) => y<=(height-3-csDErrorY)/yr
    if (border_y1<border_y0) border_y1=border_y0;
    long border_x1=(((Src.width-3)<<16)-csDErrorX)/xrIntFloat_16+1;; 
    if (border_x1<border_x0) border_x1=border_x0;

    TARGB32* pDstLine=Dst.pdata;
    long srcy_16=csDErrorY;
    long y;
    for (y=0;y<border_y0;++y)
    {
        long srcx_16=csDErrorX;
        for (long x=0;x<dst_width;++x)
        {
            ThreeOrder_Border_Common(Src,srcx_16,srcy_16,&pDstLine[x]); //border
            srcx_16+=xrIntFloat_16;
        }
        srcy_16+=yrIntFloat_16;
        ((TUInt8*&)pDstLine)+=Dst.byte_width;
    }
    for (y=border_y0;y<border_y1;++y)
    {
        long srcx_16=csDErrorX;
        long x;
        for (x=0;x<border_x0;++x)
        {
            ThreeOrder_Border_Common(Src,srcx_16,srcy_16,&pDstLine[x]);//border
            srcx_16+=xrIntFloat_16;
        }
        for (x=border_x0;x<border_x1;++x)
        {
            ThreeOrder_Fast_Common(Src,srcx_16,srcy_16,&pDstLine[x]);//fast  !
            srcx_16+=xrIntFloat_16;
        }
        for (x=border_x1;x<dst_width;++x)
        {
            ThreeOrder_Border_Common(Src,srcx_16,srcy_16,&pDstLine[x]);//border
            srcx_16+=xrIntFloat_16;
        }
        srcy_16+=yrIntFloat_16;
        ((TUInt8*&)pDstLine)+=Dst.byte_width;
    }
    for (y=border_y1;y<Dst.height;++y)
    {
        long srcx_16=csDErrorX;
        for (long x=0;x<dst_width;++x)
        {
            ThreeOrder_Border_Common(Src,srcx_16,srcy_16,&pDstLine[x]); //border
            srcx_16+=xrIntFloat_16;
        }
        srcy_16+=yrIntFloat_16;
        ((TUInt8*&)pDstLine)+=Dst.byte_width;
    }
}

#endif
