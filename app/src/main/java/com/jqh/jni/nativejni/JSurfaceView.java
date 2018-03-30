package com.jqh.jni.nativejni;

import java.nio.ByteBuffer;


import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Matrix;
import android.util.AttributeSet;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

import com.jqh.jni.imageutils.BeautifyMultiThread;
import com.jqh.jni.utils.LogUtil;

/**
 * Created by user on 2017/12/26.
 */
public class JSurfaceView extends SurfaceView  implements SurfaceHolder.Callback {

    /** 视频实际宽 */
    public Integer width = new Integer(0);
    /** 视频实际高 */
    public Integer height = new Integer(0);
    /******************** 绘图数据 ********************/
    //private boolean isDraw = false;
    private Bitmap bmpOriginal = null; // 原始视频位图
    private Bitmap bmpScale = null;// 放大后的视频位图
    private Matrix matrix = null; // 定义位图放大系数
    private ByteBuffer byteBuffer = null;
    private float fStartX = 0;
    private float fStartY = 0;
    BeautifyMultiThread beautifyMultiThread = new BeautifyMultiThread();

    public JSurfaceView(Context context) {
        super(context);
        getHolder().addCallback(this);
        getHolder().setType(SurfaceHolder.SURFACE_TYPE_PUSH_BUFFERS);
    }

    public JSurfaceView(Context context, AttributeSet attrs) {
        super(context, attrs);
        getHolder().addCallback(this);
        getHolder().setType(SurfaceHolder.SURFACE_TYPE_PUSH_BUFFERS);
    }

    public JSurfaceView(Context context, AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
        getHolder().addCallback(this);
        getHolder().setType(SurfaceHolder.SURFACE_TYPE_PUSH_BUFFERS);
    }

    private int vindex = 0 ;
    public void flushVideoData(byte[] bytes) {
        if (bytes == null)
            return;
        DrawImage(bytes);

    }
    public boolean isDraw = false ;
    public synchronized void  DrawImage(byte[] bytes) {
        Canvas canvas = null ;
        isDraw = true ;
        try {
            canvas = getHolder().lockCanvas();
            if (canvas == null)
                return;

            byteBuffer = ByteBuffer.wrap(bytes);

            bmpOriginal = Bitmap.createBitmap(width, height,
                    Bitmap.Config.RGB_565);
            bmpOriginal.copyPixelsFromBuffer(byteBuffer);
            if (matrix == null) {
                matrix = new Matrix();
                float scaleWidth = (float) canvas.getWidth()
                        / (float) bmpOriginal.getWidth();
                float scaleHeight = (float) canvas.getHeight()
                        / (float) bmpOriginal.getHeight();
                if (scaleWidth < scaleHeight) {
                    matrix.postScale(scaleWidth, scaleWidth);
                } else {
                    matrix.postScale(scaleHeight, scaleHeight);
                }
               // matrix.setRotate(90);
                bmpScale = Bitmap.createBitmap(bmpOriginal, 0, 0,
                        bmpOriginal.getWidth(), bmpOriginal.getHeight(), matrix,
                        true);
                fStartX = (float)(canvas.getWidth() - bmpScale.getWidth()) / 2;
                fStartY = (float)(canvas.getHeight() - bmpScale.getHeight()) / 2;
            }
            bmpScale = Bitmap.createBitmap(bmpOriginal, 0, 0,
                    bmpOriginal.getWidth(), bmpOriginal.getHeight(), matrix,
                    true);
            // 美颜处理
            //bmpScale = beautifyMultiThread.beautifyImg(bmpScale,20) ;
            if(bmpScale != null)
            {
                synchronized(bmpScale)
                {
                    LogUtil.i("canvas->drawBitmap");
                    canvas.drawBitmap(bmpScale, fStartX, fStartY, null);
                }
            }

            if(bmpOriginal.isRecycled() == false)
            {
                bmpOriginal.recycle();
            }
            // 释放
            if(bmpScale.isRecycled() == false)
            {
                bmpScale.recycle();
            }
            bmpOriginal = null;
            bmpScale = null;
        } catch (Exception e) {
            LogUtil.i("DrawImage exception msg "+ e.getMessage());
        } finally
        {
            if(getHolder()!= null && canvas != null)
            {
                try
                {
                    getHolder().unlockCanvasAndPost(canvas);//结束锁定画图，并提交改变。
                }catch(Exception e)
                {

                }
                isDraw = false;
            }
        }
    }

    public void cleanSutface()
    {
        matrix = null ;
    }
    @Override
    public void surfaceCreated(SurfaceHolder holder) {
    }

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width,
                               int height) {
    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
    }
}
