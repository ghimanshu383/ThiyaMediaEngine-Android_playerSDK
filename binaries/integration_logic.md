## SDK Integration Example

The SDK expects a **detached file descriptor (fd)** to be passed to the player Activity.

Detaching the file descriptor transfers ownership of the file handle to the native
engine so that the C++ media pipeline can manage the resource lifecycle safely.

The following example demonstrates how to open a video using the Android file picker
and pass the detached file descriptor to `ThiyaPlayer`.

### Sample Kotlin integration for gallery videos

registerForActivityResult(ActivityResultContracts.StartActivityForResult()) { result ->
    if (result.resultCode == Activity.RESULT_OK) {
        if (result.data != null && result.data?.clipData != null) {

            val uri = result.data?.clipData?.getItemAt(0)?.uri!!

            val pfd = contentResolver.openFileDescriptor(uri, "r")
            if (pfd == null) {
                Log.e("MEDIA", "Failed to open ParcelFileDescriptor")
                return@registerForActivityResult
            }

            // Detach the file descriptor so that the native engine
            // can take ownership of the resource.
            val fd = pfd.detachFd()

            val intent = Intent(this@StartupActivity, ThiyaPlayer::class.java)
            intent.putExtra(ThiyaPlayerConstants.fdString, fd)

            startActivity(intent)
        }
    }
}

##  Important: Once `detachFd()` is called, the Java layer no longer owns the file descriptor. The native engine becomes responsible for managing and closing it.
