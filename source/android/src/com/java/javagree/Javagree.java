package com.java.javagree;

import java.io.IOException;
import java.io.InputStream;
import java.util.Arrays;

import android.app.Activity;
import android.graphics.Point;
import android.graphics.Rect;
import android.text.TextPaint;
import android.util.Log;
import android.util.SparseArray;
import android.view.Display;
import android.view.View;
import android.view.View.OnClickListener;
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
class Javagree implements OnClickListener {

	private final static int FUDGE = 30;
	private int id = 1001;
	Object callback;
	String name;
	private SparseArray<Object> actionifiers = new SparseArray<Object>();

	Javagree(Object callback, String name) {
		this.callback = callback;
		this.name = name;
	}

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
	private native int evalSource(Object callback, String name, String source, Object sys, Object... args);
	private native int evalBytes(Object callback, String name, byte[] bytes, Object sys, Object... args);

	int eval(String source, Object... args) {
		return this.evalSource(this.callback, this.name, source, this, args);
	}

	int eval(byte[] bytes, Object... args) {
		return this.evalBytes(this.callback, this.name, bytes, this, args);
	}

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

	public Object[] table(Object uictx, Object x, Object y, Object w, Object h,  Object[] values, byte[] logic) {
		Activity activity = App.getCurrentActivity();
		ListView lv = new ListView(activity);
		String[] values2 = Arrays.copyOf(values, values.length, String[].class);
		ArrayAdapter<String> adapter = new ArrayAdapter<String>(activity, android.R.layout.simple_list_item_1, values2);		 
	    lv.setAdapter(adapter);
		return this.putView(lv, uictx, (Integer)x, (Integer)y, (Integer)w, (Integer)h, logic);
	}

	public Object[] input(Object uictx, Object x, Object y) {
		MainActivity activity = App.getCurrentActivity();
		EditText et = new EditText(activity);
		int h = (int)et.getTextSize();
		int frameWidth = this.window(0, 0)[0];
		int w = frameWidth - (Integer)x - h;
		RelativeLayout.LayoutParams params = new RelativeLayout.LayoutParams(RelativeLayout.LayoutParams.MATCH_PARENT, RelativeLayout.LayoutParams.WRAP_CONTENT);
		return this.putView(et, uictx, (Integer)x, (Integer)y, w, h + FUDGE, params, null);
	}

	public Object[] button(Object uictx, Object x, Object y, byte[] logic, String text, String image) {
		Log.d("Javagree", "button: " + uictx + " "+ x +","+ y +", logic=" + logic + ", text=" + text + ", image=" + image);
		Activity activity = App.getCurrentActivity();
		Button b = new Button(activity);
		b.setOnClickListener(this);
		return this.putTextView(b, uictx, (Integer)x, (Integer)y, text, logic);
	}

	public Object[] label(Object uictx, Object x, Object y, String text) {		
		Activity activity = App.getCurrentActivity();
		TextView tv = new TextView(activity);
		return this.putTextView(tv, uictx, (Integer)x, (Integer)y, text, null);
	}

	public Object[] ui_set(Object id, Object value) {
		MainActivity activity = App.getCurrentActivity();
		View v = activity.findViewById((Integer)id);
		if (v instanceof TextView)
			((TextView)v).setText((String)value);
		return new Object[]{};
	}
	
	private Object[] putTextView(TextView tv, Object uictx, int x, int y, String text, byte[] logic) {

		tv.setText(text);

		Rect bounds = new Rect();
		TextPaint paint = tv.getPaint();
		paint.getTextBounds(text, 0, text.length(), bounds);
		int width = bounds.width() + tv.getPaddingLeft() + tv.getPaddingRight() + FUDGE;
		int height = bounds.height() + tv.getPaddingBottom() + tv.getPaddingTop() + FUDGE;

		return this.putView(tv, uictx, x, y, width, height, logic);
	}

	private Object[] putView(View v, Object uictx, int x, int y, int width, int height, byte[] logic) {
		v.setId(this.id++);
		RelativeLayout.LayoutParams params = new RelativeLayout.LayoutParams(width, height);
		return this.putView(v, uictx, x, y, width, height, params, logic);
	}
	
	private Object[] putView(View v, Object uictx, int x, int y, int width, int height, RelativeLayout.LayoutParams params, byte[] logic) {
		v.setId(this.id++);

		params.leftMargin = (Integer)x;
		params.topMargin = (Integer)y;
		MainActivity activity = App.getCurrentActivity();
		RelativeLayout layout = activity.getLayout();
		layout.addView(v, params);

		if (logic != null) {
			Actionifier a = new Actionifier(uictx, logic);
			Log.d("Javagree", "put actionifiers[ " + v.getId() + " ]");
			this.actionifiers.put(v.getId(), a); 
		}

		return new Object[]{v.getId(), width, height};
	}

	public void onClick(View v) {
		Actionifier a = (Actionifier) this.actionifiers.get(v.getId());
		byte[] logic = a.getLogic();
		if (logic != null)
			this.eval(logic, a.getUictx());
	}

	private class Actionifier {

		private byte[] logic;
		private Object uictx;

		public Actionifier(Object uictx, byte[] logic) {
			this.uictx = uictx;
			this.logic = logic;
		}

		Object getUictx()	{ return this.uictx; }
		byte[] getLogic()	{ return this.logic; }
	}
}
