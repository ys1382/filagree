package com.java.javagree;

/**
 *
 * Javagree
 *
 * filagree -> OS bridge. Should be extended to implement Android-specific HAL functions.
 * 
 */
class Javagree {

	Object callback;
	String name;
	Object sys;

	Javagree(Object callback, String name, Object sys) {
		this.callback = callback;
		this.name = name;
		this.sys = sys;
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
		return this.evalSource(this.callback, this.name, source, this.sys, args);
	}

	int eval(byte[] bytes, Object... args) {
		return this.evalBytes(this.callback, this.name, bytes, this.sys, args);
	}

	/////// private members

	static {
		System.loadLibrary("javagree"); 
	}
}