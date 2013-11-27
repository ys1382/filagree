package com.java.javagree;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.util.Arrays;

import android.app.Activity;
import android.graphics.Point;
import android.graphics.Rect;
import android.os.Environment;
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
 * HalAndroid
 *
 * implements Android-specific HAL functions.
 * 
 */
class HalAndroid implements OnClickListener {

	private final static int FUDGE = 30;
	private static int id = 1001;
	private SparseArray<Object> actionifiers = new SparseArray<Object>();
	private SparseArray<Object> views = new SparseArray<Object>();
	private Javagree javagree;

	void setJavagree(Javagree jg) {
		this.javagree = jg;
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

	private File localFile(String path) {
		return new File(Environment
				.getExternalStorageDirectory()
				.getAbsolutePath()
				+"/"+ path);
	}

	public Object[] file_list(String path) {
		return localFile(path).list();
	}

	public Object[] rm(String path) {
		localFile(path).delete();
		return new Object[]{};
	}

	public Object[] mkdir(String path) {
		localFile(path).mkdir();
		return new Object[]{};
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

	public Object[] table(Object uictx, Object[] values, byte[] logic) {
		Activity activity = App.getCurrentActivity();
		ListView lv = new ListView(activity);
		Object[] values2 = Arrays.copyOf(values, values.length, Object[].class);
		ArrayAdapter<Object> adapter = new ArrayAdapter<Object>(activity, android.R.layout.simple_list_item_1, values2);		 
	    lv.setAdapter(adapter);
		return this.makeView(lv, uictx, 0, 0, logic);
	}

	public Object[] input(Object uictx) {
		MainActivity activity = App.getCurrentActivity();
		EditText et = new EditText(activity);
		int h = (int)et.getTextSize();
		int frameWidth = this.window(0, 0)[0];
		int w = frameWidth / 2;
		return this.makeView(et, uictx, w, h + FUDGE, null);
	}

	public Object[] button(Object uictx, byte[] logic, String text, String image) {
		Log.d("Javagree", "button: " + uictx + ", logic=" + logic + ", text=" + text + ", image=" + image);
		Activity activity = App.getCurrentActivity();
		Button b = new Button(activity);
		b.setOnClickListener(this);
		return this.putTextView(b, uictx, logic, text);
	}

	public Object[] label(Object uictx, String text) {		
		Activity activity = App.getCurrentActivity();
		TextView tv = new TextView(activity);
		return this.putTextView(tv, uictx, null, text);
	}

	public Object[] ui_set(Object id, Object value) {
		MainActivity activity = App.getCurrentActivity();
		View v = activity.findViewById((Integer)id);
		if (v instanceof TextView)
			((TextView)v).setText((String)value);
		return new Object[]{};
	}

	private Object[] putTextView(TextView tv, Object uictx, byte[] logic, String text) {

		tv.setText(text);

		Rect bounds = new Rect();
		TextPaint paint = tv.getPaint();
		paint.getTextBounds(text, 0, text.length(), bounds);
		int width = bounds.width() + tv.getPaddingLeft() + tv.getPaddingRight() + FUDGE;
		int height = bounds.height() + tv.getPaddingBottom() + tv.getPaddingTop() + FUDGE;

		return this.makeView(tv, uictx, width, height, logic);
	}

	private Object[] makeView(View v, Object uictx, int width, int height, byte[] logic) {

		v.setId(HalAndroid.id++);

		if (logic != null) {
			Actionifier a = new Actionifier(uictx, logic);
			Log.d("Javagree", "put actionifiers[ " + v.getId() + " ]");
			this.actionifiers.put(v.getId(), a); 
		}
		this.views.put(v.getId(), v); 

		return new Object[]{v.getId(), width, height};
	}
	
	public Object[] ui_put(Object widget, Object x, Object y, Object w, Object h) {

		View v = (View)this.views.get((Integer)widget);
		RelativeLayout.LayoutParams params;
		if (v.getClass() == EditText.class)
			params = new RelativeLayout.LayoutParams(RelativeLayout.LayoutParams.MATCH_PARENT, RelativeLayout.LayoutParams.WRAP_CONTENT);
		else
			 params = new RelativeLayout.LayoutParams((Integer)w, (Integer)h);

		params.leftMargin = (Integer)(x == null ? 0 : x);
		params.topMargin = (Integer)y;
		MainActivity activity = App.getCurrentActivity();
		RelativeLayout layout = activity.getLayout();
		layout.addView(v, params);

		return new Object[]{};
}

	public void onClick(View v) {
		Actionifier a = (Actionifier) this.actionifiers.get(v.getId());
		byte[] logic = a.getLogic();
		if (logic != null)
			this.javagree.eval(logic, a.getUictx());
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
