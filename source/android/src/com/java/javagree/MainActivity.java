package com.java.javagree;

import com.java.javagree.R;

import android.app.Activity;
import android.os.Bundle;
import android.view.Menu;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.RelativeLayout;

public class MainActivity extends Activity implements OnClickListener {

	RelativeLayout layout;
	RelativeLayout getLayout() {
		return this.layout;
	}


	@Override
	public void onClick(View v) {
		/*    	if(v.getId()==R.id.button1) {
     	       Javagree.doTesting(this);
    	} */
	}

	@Override
	public boolean onCreateOptionsMenu(Menu menu) {
		// Inflate the menu; this adds items to the action bar if it is present.
		getMenuInflater().inflate(R.menu.main, menu);
		return true;
	}

	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		App.setCurrentActivity(this);
		setContentView(R.layout.activity_main);
		this.layout = (RelativeLayout)findViewById(R.id.main_layout);
		Test.doTesting();
	}
}
