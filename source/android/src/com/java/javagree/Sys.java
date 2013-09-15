package com.java.javagree;

import java.io.IOException;
import java.io.InputStream;

import android.util.Log;
import android.widget.Button;
import android.widget.RelativeLayout;

/**
*
* Sys
*
* filagree -> OS bridge. Should be extended to implement Android-specific HAL functions.
* 
*/
class Sys {

   private MainActivity activity;

   public Sys(MainActivity activity) { this.activity = activity; }

   public String read(String name) {
	   
	   Log.d("Javagree", "read " + name);

       InputStream input;
       
       try {

    	   input = this.activity.getAssets().open(name);

           int size = input.available();
           byte[] buffer = new byte[size];
           input.read(buffer);
           input.close();

          return new String(buffer);

       } catch (IOException e) {
           e.printStackTrace();
           return "";
       }
   }

   public Integer[] window(int w, int h) {
       return new Integer[]{600,800};
   }

   public Object[] button(Object uictx,
		   Object x, Object y, Object w, Object h,
           String logic, String text, String image) {
	   Integer x2 = (Integer)x;
       Log.d("filagree", "button " + x2 +","+ y +","+ w +","+ h +", logic="+ logic +", text="+ text +", image="+ image);

       Button valueB = new Button(this.activity);
       valueB.setText(text);

       RelativeLayout.LayoutParams params = new RelativeLayout.LayoutParams(100, 200);
       params.leftMargin = (Integer)x;
       params.topMargin = (Integer)y;
       this.activity.layout.addView(valueB, params);
       return new Object[]{null,22,33};
   }

   public Object[] label(Object x, Object y, String text) {

	   Integer x2 = (Integer)x;
	   Integer y2 = (Integer)y;
	   Log.d("Sys", "label " + x2 +","+ y2 + ": " + text);
	   FitTextView ftv = new FitTextView(this.activity, text);
       this.activity.layout.addView(ftv);

       return new Object[]{ftv.getId(), ftv.getWidth(), ftv.getHeight()};
   }
}
