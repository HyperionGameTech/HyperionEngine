//! What every generated wrapper in [`crate::engine`] is built on.

use crate::sys;
use std::ffi::{c_void, CString};
use std::ops::Deref;

/// A wrapper around a pointer to an engine object.
///
/// A wrapper on its own is *borrowed*: it is valid while the engine keeps the object alive (a node while it is in
/// its scene, the world while the game runs). Nothing checks that for you. To keep an object beyond the hook you got
/// it in, [`retain`](EngineObject::retain) it.
///
/// # Safety
/// `Raw` must be the engine type the pointer refers to, and the type must be a transparent wrapper around it.
pub unsafe trait EngineObject: Copy + Sized {
    type Raw;

    fn as_ptr(&self) -> *mut Self::Raw;

    /// # Safety
    /// `ptr` must be null or point to a live engine object of this type.
    unsafe fn from_ptr(ptr: *mut Self::Raw) -> Option<Self>;

    /// Takes a reference to the object, keeping it alive until the returned value is dropped.
    fn retain(&self) -> Owned<Self> {
        unsafe { sys::Hyp_Retain(self.as_ptr() as *mut c_void) };

        Owned(*self)
    }
}

/// An engine object this code holds a reference to. Dropping it gives the reference back.
///
/// Engine methods that hand out a new reference (for example `Node::get_child`) return this.
pub struct Owned<T: EngineObject>(T);

impl<T: EngineObject> Owned<T> {
    /// # Safety
    /// `ptr` must be null or an object the caller holds a reference to; that reference moves into the result.
    pub unsafe fn from_retained(ptr: *mut T::Raw) -> Option<Self> {
        unsafe { T::from_ptr(ptr) }.map(Owned)
    }

    /// Gives up the reference without releasing it and returns the raw pointer.
    pub fn into_raw(self) -> *mut T::Raw {
        let ptr = self.0.as_ptr();
        std::mem::forget(self);

        ptr
    }
}

impl<T: EngineObject> Deref for Owned<T> {
    type Target = T;

    fn deref(&self) -> &T {
        &self.0
    }
}

impl<T: EngineObject> Clone for Owned<T> {
    fn clone(&self) -> Self {
        self.0.retain()
    }
}

impl<T: EngineObject> Drop for Owned<T> {
    fn drop(&mut self) {
        unsafe { sys::Hyp_Release(self.0.as_ptr() as *mut c_void) };
    }
}

impl<T: EngineObject + std::fmt::Debug> std::fmt::Debug for Owned<T> {
    fn fmt(&self, formatter: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        formatter.debug_tuple("Owned").field(&self.0).finish()
    }
}

/// A string for the engine, which reads up to the first NUL.
pub fn to_c_string(string: &str) -> CString {
    let end = string.find('\0').unwrap_or(string.len());

    CString::new(&string[..end]).unwrap_or_default()
}

/// Copies an engine-allocated string and frees it.
///
/// # Safety
/// `string` must have been filled in by an engine binding and not freed yet.
pub unsafe fn take_string(string: sys::HypString) -> String {
    if string.data.is_null() {
        return String::new();
    }

    let bytes = unsafe { std::slice::from_raw_parts(string.data as *const u8, string.length as usize) };
    let result = String::from_utf8_lossy(bytes).into_owned();

    unsafe { sys::Hyp_Free(string.data as *mut c_void) };

    result
}

/// Copies an engine-allocated array and frees it.
///
/// # Safety
/// `array` must have been filled in by an engine binding and not freed yet.
pub unsafe fn take_array<T: Copy>(array: sys::HypArray<T>) -> Vec<T> {
    if array.data.is_null() {
        return Vec::new();
    }

    let result = unsafe { std::slice::from_raw_parts(array.data, array.length as usize) }.to_vec();

    unsafe { sys::Hyp_Free(array.data as *mut c_void) };

    result
}
