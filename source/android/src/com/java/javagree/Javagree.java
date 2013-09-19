package com.java.javagree;

import java.io.IOException;
import java.io.InputStream;
import java.util.Arrays;

import android.app.Activity;
import android.graphics.Point;
import android.graphics.Rect;
import android.text.TextPaint;
import android.util.Log;
import android.view.Display;
import android.view.View;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.EditText;
import android.widget.ListView;
import android.widget.RelativeLayout;
import android.widget.TextView;

/**
 *
 * Javagree
 *
 * filagree -> OS bridge. Should be extended to implement Android-specific HAL functions.
 * 
 */
class Javagree {

	private final static int FUDGE = 30;
	private int id = 1001;

	/**
	 *
	 * Evaluates string in filagree
	 *
	 * @param callback object
	 * @param name of callback object
	 * @param program program
	 * @param sys implements HAL platform API
	 *
	 */
	public native int eval(Object callback, String name, String program, Object sys);

	/////// private members

	static {
		System.loadLibrary("javagree"); 
	}


	public String read(String name) {
		try {
			Activity activity = App.getCurrentActivity();
			InputStream input = activity.getAssets().open(name);
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
		Activity activity = App.getCurrentActivity();
		Display display = activity.getWindowManager().getDefaultDisplay();
		Point size = new Point();
		display.getSize(size);
		int width = size.x;
		int height = size.y;
		return new Integer[]{width, height};
	}

	public Object[] table(Object uictx, Object x, Object y, Object w, Object h,  Object[] values, String logic) {
		Activity activity = App.getCurrentActivity();
		ListView lv = new ListView(activity);
		String[] values2 = Arrays.copyOf(values, values.length, String[].class);
		ArrayAdapter<String> adapter = new ArrayAdapter<String>(activity, android.R.layout.simple_list_item_1, values2);		 
	    lv.setAdapter(adapter);
		return this.putView(lv, (Integer)x, (Integer)y, (Integer)w, (Integer)h);
	}

	public Object[] input(Object uictx, Object x, Object y) {
		MainActivity activity = App.getCurrentActivity();
		EditText et = new EditText(activity);
		int h = (int)et.getTextSize();
		int frameWidth = this.window(0, 0)[0];
		int w = frameWidth - (Integer)x - h;
		RelativeLayout.LayoutParams params = new RelativeLayout.LayoutParams(RelativeLayout.LayoutParams.MATCH_PARENT, RelativeLayout.LayoutParams.WRAP_CONTENT);
		return this.putView(et, (Integer)x, (Integer)y, w, h + FUDGE, params);
	}

	public Object[] button(Object uictx, Object x, Object y, String logic, String text, String image) {
		Log.d("Javagree", "button: " + uictx + " "+ x +","+ y +", logic=" + logic + ", text=" + text + ", image=" + image);
		Activity activity = App.getCurrentActivity();
		Button b = new Button(activity);
		return this.putTextView(b, (Integer)x, (Integer)y, text);
	}

	public Object[] label(Object x, Object y, String text) {		
		Activity activity = App.getCurrentActivity();
		TextView tv = new TextView(activity);
		return this.putTextView(tv, (Integer)x, (Integer)y, text);
	}

	private Object[] putTextView(TextView tv, int x, int y, String text) {

		tv.setText(text);

		Rect bounds = new Rect();
		TextPaint paint = tv.getPaint();
		paint.getTextBounds(text, 0, text.length(), bounds);
		int width = bounds.width() + tv.getPaddingLeft() + tv.getPaddingRight() + FUDGE;
		int height = bounds.height() + tv.getPaddingBottom() + tv.getPaddingTop() + FUDGE;

		return this.putView(tv, x, y, width, height);
	}

	private Object[] putView(View v, int x, int y, int width, int height) {
		v.setId(this.id++);
		RelativeLayout.LayoutParams params = new RelativeLayout.LayoutParams(width, height);
		return this.putView(v, x, y, width, height, params);
	}
	
	private Object[] putView(View v, int x, int y, int width, int height, RelativeLayout.LayoutParams params) {
		params.leftMargin = (Integer)x;
		params.topMargin = (Integer)y;
		MainActivity activity = App.getCurrentActivity();
		RelativeLayout layout = activity.getLayout();
		layout.addView(v, params);
		return new Object[]{v.getId(), width, height};
	}
}
