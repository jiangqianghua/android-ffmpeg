package com.jqh.jni.demojni;
import java.util.Collections;
import java.util.Comparator;
import java.util.List;

import android.hardware.Camera;
import android.hardware.Camera.Size;

import com.jqh.jni.utils.LogUtil;

/**
 * Created by user on 2018/1/17.
 */
public class MyCamPara {
    private final CameraSizeComparator sizeComparator = new CameraSizeComparator();
    private static MyCamPara myCamPara = null;
    private MyCamPara(){

    }
    public static MyCamPara getInstance(){
        if(myCamPara == null){
            myCamPara = new MyCamPara();
            return myCamPara;
        }
        else{
            return myCamPara;
        }
    }

    public  Size getPreviewSize(List<Camera.Size> list, int th){
        Collections.sort(list, sizeComparator);
        Size size=null;
        for(int i=0;i<list.size();i++){
            size=list.get(i);
            if((size.width>th)&&equalRate(size, 1.3f)){
                break;
            }
        }
        return size;
    }
    public Size getPictureSize(List<Camera.Size> list, int th){
        Collections.sort(list, sizeComparator);
        Size size=null;
        for(int i=0;i<list.size();i++){
            size=list.get(i);
            if((size.width>th)&&equalRate(size, 1.3f)){
                break;
            }
        }
        return size;

    }

    public boolean equalRate(Size s, float rate){
        float r = (float)(s.width)/(float)(s.height);
        if(Math.abs(r - rate) <= 0.2)
        {
            return true;
        }
        else{
            return false;
        }
    }

    public  class CameraSizeComparator implements Comparator<Camera.Size>{
        //按升序排列
        @Override
        public int compare(Size lhs, Size rhs) {
            // TODO Auto-generated method stub
            if(lhs.width == rhs.width){
                return 0;
            }
            else if(lhs.width > rhs.width){
                return 1;
            }
            else{
                return -1;
            }
        }

    }

    public static void showSupportedPreviewFpsRange(Camera.Parameters parameters){
        List<int[]> range=parameters.getSupportedPreviewFpsRange();
        LogUtil.d("range:"+range.size());
        for(int j=0;j<range.size();j++) {
            int[] r=range.get(j);
            for(int k=0;k<r.length;k++) {
                LogUtil.d(","+r[k]);
            }
            LogUtil.d("--------");
        }
    }

}