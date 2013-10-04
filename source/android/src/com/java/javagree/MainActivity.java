package com.java.javagree;

import com.java.javagree.R;

import android.app.Activity;
import android.os.Bundle;
import android.view.Menu;
import android.widget.RelativeLayout;

public class MainActivity extends Activity {

	RelativeLayout layout;
	RelativeLayout getLayout() {
		return this.layout;
	}

	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		App.setCurrentActivity(this);
		setContentView(R.layout.activity_main);
		this.layout = (RelativeLayout)findViewById(R.id.main_layout);
		Test test = new Test();
		test.doTesting();
	}

	@Override
	public boolean onCreateOptionsMenu(Menu menu) {
		getMenuInflater().inflate(R.menu.main, menu);
		return true;
	}
}
