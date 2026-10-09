package org.ra2.engine;

import android.app.Activity;
import android.content.ContentResolver;
import android.content.Intent;
import android.database.Cursor;
import android.graphics.Color;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.provider.DocumentsContract;
import android.util.Log;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.ScrollView;
import android.widget.TextView;

import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;

/**
 * W2-AR-03 / W2-AR-04: the launcher activity. It checks that the user's own
 * Red Alert 2 files are in the app's external files dir
 * (Android/data/org.ra2.engine/files/ra2/, the folder the engine reads) and
 * starts the game when they are. Otherwise it explains what is missing and
 * offers an in-app import: the Storage Access Framework folder picker
 * (ACTION_OPEN_DOCUMENT_TREE -- works on Android 5 to 15, no storage
 * permission) copies the files in under their canonical names. That replaces
 * the old "adb push into Android/data" recipe, which Android 11+ refuses for
 * the shell user and file managers.
 *
 * Runs in its own process (":setup", see the manifest) so the engine's
 * exit() at the end of main() cannot take it down when the engine hands back
 * to it (the missing-files page's IMPORT button).
 */
public class SetupActivity extends Activity {
    private static final String TAG = "RA2Setup";
    private static final int REQ_TREE = 1;
    public static final String EXTRA_IMPORT = "org.ra2.engine.IMPORT";
    /** W6-06-05: the launcher shortcut ("Import game files", res/xml/shortcuts.xml). */
    public static final String ACTION_IMPORT = "org.ra2.engine.action.IMPORT";

    /** The files the engine reads; the first four are required (engine/data_dir.inc). */
    static final String[] FILES = {
        "ra2.mix", "ra2md.mix", "language.mix", "multimd.mix",
        "langmd.mix", "mapsmd03.mix", "theme.mix", "expand01.mix",
        "Maps01.mix", "MAPS02.MIX", "ra2.csf",
    };
    static final int REQUIRED = 4;

    /**
     * W6-06-06: the file a picked document is copied to (relative to the data
     * dir), or null when the engine does not read it. The fixed FILES keep
     * their retail spelling; the patch archives the desktop AssetFS scans
     * (src/asset_fs.cpp: expandNN / ecacheNN / elocalNN), thememd.mix and the
     * movies (MOVIESnn.MIX / MOVMDnn.MIX archives and loose .bik files, which
     * engine/movie_player.inc finds in the data dir and its Movies/ folder,
     * case-insensitively) are copied too.
     */
    static String target(String name) {
        if (name == null) return null;
        String l = name.toLowerCase(Locale.ROOT);
        for (String f : FILES) if (f.toLowerCase(Locale.ROOT).equals(l)) return f;
        if (l.matches("(expand|ecache|elocal)\\d\\d\\.mix")) return l;
        if (l.equals("thememd.mix")) return l;
        if (l.matches("(movies\\d\\d|movmd\\d\\d)\\.mix")) return l;
        if (l.endsWith(".bik") && l.length() > 4) return "Movies/" + l;
        return null;
    }

    private TextView status, title;
    private ProgressBar progress;
    private Button importButton, startButton, exitButton;
    private final Handler ui = new Handler(Looper.getMainLooper());
    private volatile boolean copying = false;
    // R-161: the copy thread is owned by the activity; leaving (finish /
    // destroy) cancels it, and its partial file is deleted.
    private Thread importThread;
    private volatile boolean cancelImport = false;

    static File dataDir(Activity a) {
        File ext = a.getExternalFilesDir(null);
        File d = new File(ext != null ? ext : a.getFilesDir(), "ra2");
        //noinspection ResultOfMethodCallIgnored
        d.mkdirs();   // visible over MTP even before the first import
        return d;
    }

    static List<String> missing(File dir, boolean requiredOnly) {
        List<String> m = new ArrayList<>();
        for (int i = 0; i < (requiredOnly ? REQUIRED : FILES.length); ++i) {
            File f = new File(dir, FILES[i]);
            if (!f.isFile() || f.length() == 0) m.add(FILES[i]);
        }
        return m;
    }

    /** R-161: delete *.part leftovers of an interrupted copy (app killed mid-import). */
    static int cleanParts(File dir) {
        int n = 0;
        File[] list = dir.listFiles();
        if (list == null) return 0;
        for (File f : list) {
            if (f.isFile() && f.getName().endsWith(".part") && f.delete()) ++n;
            else if (f.isDirectory() && f.getName().equals("Movies")) n += cleanParts(f);
        }
        if (n > 0) Log.i(TAG, "removed " + n + " partial file(s) of an interrupted import");
        return n;
    }

    @Override
    protected void onCreate(Bundle state) {
        super.onCreate(state);
        File dir = dataDir(this);
        cleanParts(dir);
        Intent in = getIntent();
        boolean forceImport = in != null && (in.getBooleanExtra(EXTRA_IMPORT, false) || ACTION_IMPORT.equals(in.getAction()));
        if (!forceImport && missing(dir, true).isEmpty()) {
            startGame();
            return;
        }
        buildUi();
        refresh();
    }

    private int dp(int v) {
        return (int) TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, v, getResources().getDisplayMetrics());
    }

    private Button button(String label, View.OnClickListener l) {
        Button b = new Button(this);
        b.setText(label);
        b.setMinHeight(dp(56));   // >= 48 dp touch target
        b.setOnClickListener(l);
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f);
        lp.setMargins(dp(6), dp(6), dp(6), dp(6));
        b.setLayoutParams(lp);
        return b;
    }

    private void buildUi() {
        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setBackgroundColor(Color.BLACK);
        root.setPadding(dp(24), dp(16), dp(24), dp(16));

        title = new TextView(this);
        title.setText(R.string.setup_title);
        title.setTextColor(Color.rgb(255, 200, 90));
        title.setTextSize(TypedValue.COMPLEX_UNIT_SP, 22);
        root.addView(title);

        ScrollView scroll = new ScrollView(this);
        status = new TextView(this);
        status.setTextColor(Color.rgb(220, 220, 220));
        status.setTextSize(TypedValue.COMPLEX_UNIT_SP, 15);
        status.setPadding(0, dp(8), 0, dp(8));
        scroll.addView(status);
        root.addView(scroll, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, 0, 1f));

        progress = new ProgressBar(this, null, android.R.attr.progressBarStyleHorizontal);
        progress.setMax(1000);
        progress.setVisibility(View.GONE);
        root.addView(progress);

        LinearLayout row = new LinearLayout(this);
        row.setOrientation(LinearLayout.HORIZONTAL);
        row.setGravity(Gravity.CENTER);
        importButton = button(getString(R.string.setup_import), v -> pickFolder());
        startButton = button(getString(R.string.setup_start), v -> startGame());
        exitButton = button(getString(R.string.setup_exit), v -> finish());
        row.addView(importButton);
        row.addView(startButton);
        row.addView(exitButton);
        root.addView(row);
        setContentView(root);
    }

    private void refresh() {
        File dir = dataDir(this);
        List<String> req = missing(dir, true);
        List<String> all = missing(dir, false);
        StringBuilder sb = new StringBuilder();
        sb.append(getString(R.string.setup_intro)).append("\n\n");
        sb.append(getString(R.string.setup_folder)).append("\n  ").append(dir.getAbsolutePath()).append("\n\n");
        // W6-06-05: opened to add / replace files with everything present.
        title.setText(req.isEmpty() ? R.string.setup_title_ready : R.string.setup_title);
        if (req.isEmpty()) {
            sb.append(getString(R.string.setup_ready)).append("\n");
        } else {
            sb.append(getString(R.string.setup_missing_required)).append("\n");
            for (String f : req) sb.append("  ").append(f).append("\n");
        }
        List<String> opt = new ArrayList<>(all);
        opt.removeAll(req);
        if (!opt.isEmpty()) {
            sb.append("\n").append(getString(R.string.setup_missing_optional)).append("\n");
            for (String f : opt) sb.append("  ").append(f).append("\n");
        }
        sb.append("\n").append(getString(R.string.setup_how));
        sb.append("\n\n").append(getString(R.string.setup_reimport_note));   // W6-06-05
        status.setText(sb.toString());
        startButton.setEnabled(req.isEmpty() && !copying);
        importButton.setEnabled(!copying);
        exitButton.setEnabled(!copying);   // R-161: no Exit in the middle of a copy
    }

    /** R-161: Back does not abandon a running copy either. */
    @Override
    public void onBackPressed() {
        if (copying) return;
        super.onBackPressed();
    }

    @Override
    protected void onDestroy() {
        if (importThread != null && importThread.isAlive()) {
            cancelImport = true;   // the thread deletes its .part file and stops
            try { importThread.join(2000); } catch (InterruptedException ignored) { }
        }
        super.onDestroy();
    }

    private void pickFolder() {
        Intent i = new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE);
        i.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
        // R-160: open the picker at Download/RA2 (where the instructions say
        // to put the files). Android 11+ refuses Download itself as a tree,
        // so the user must open the RA2 sub-folder and tap USE THIS FOLDER.
        if (Build.VERSION.SDK_INT >= 26) {
            i.putExtra(DocumentsContract.EXTRA_INITIAL_URI, DocumentsContract.buildDocumentUri(
                    "com.android.externalstorage.documents", "primary:Download/RA2"));
        }
        startActivityForResult(i, REQ_TREE);
    }

    @Override
    protected void onActivityResult(int req, int res, Intent data) {
        super.onActivityResult(req, res, data);
        if (req != REQ_TREE || res != RESULT_OK || data == null || data.getData() == null) return;
        final Uri tree = data.getData();
        copying = true;
        cancelImport = false;
        progress.setVisibility(View.VISIBLE);
        progress.setProgress(0);
        refresh();
        importThread = new Thread(() -> importFrom(tree), "ra2-import");
        importThread.start();
    }

    /**
     * Lists the game files directly inside `docId` (case-insensitive, see
     * target()); sub-folders go to `subdirs` (document id, display name).
     */
    private void scan(ContentResolver cr, Uri tree, String docId,
                      Map<String, Uri> found, Map<String, Long> sizes, List<String[]> subdirs) {
        Uri children = DocumentsContract.buildChildDocumentsUriUsingTree(tree, docId);
        try (Cursor c = cr.query(children, new String[]{
                DocumentsContract.Document.COLUMN_DOCUMENT_ID,
                DocumentsContract.Document.COLUMN_DISPLAY_NAME,
                DocumentsContract.Document.COLUMN_SIZE,
                DocumentsContract.Document.COLUMN_MIME_TYPE}, null, null, null)) {
            while (c != null && c.moveToNext()) {
                String name = c.getString(1);
                if (DocumentsContract.Document.MIME_TYPE_DIR.equals(c.getString(3))) {
                    if (subdirs != null) subdirs.add(new String[]{c.getString(0), name == null ? "" : name});
                    continue;
                }
                String key = target(name);
                if (key == null || found.containsKey(key)) continue;
                found.put(key, DocumentsContract.buildDocumentUriUsingTree(tree, c.getString(0)));
                sizes.put(key, c.isNull(2) ? 0L : c.getLong(2));
            }
        }
    }

    /** Copies every known file found in the picked folder (case-insensitive match). */
    private void importFrom(Uri tree) {
        ContentResolver cr = getContentResolver();
        File dir = dataDir(this);
        Map<String, Uri> found = new HashMap<>();
        Map<String, Long> sizes = new HashMap<>();
        long total = 0;
        String error = null;
        try {
            // R-160: the picked folder itself, then (when it holds none of the
            // required files) one level of sub-folders -- picking the folder
            // that CONTAINS RA2 (e.g. the copied RA2-phone package) works too.
            List<String[]> subdirs = new ArrayList<>();
            scan(cr, tree, DocumentsContract.getTreeDocumentId(tree), found, sizes, subdirs);
            boolean anyRequired = false;
            for (int i = 0; i < REQUIRED; ++i) anyRequired |= found.containsKey(FILES[i]);
            List<String[]> deeper = new ArrayList<>();
            for (String[] sub : subdirs) {
                // W6-06-06: a Movies folder next to the files is always read;
                // R-160: other sub-folders only when the picked folder holds
                // none of the required files (the folder that CONTAINS RA2).
                if (!anyRequired || sub[1].equalsIgnoreCase("movies"))
                    scan(cr, tree, sub[0], found, sizes, anyRequired ? null : deeper);
            }
            for (String[] sub : deeper)
                if (sub[1].equalsIgnoreCase("movies")) scan(cr, tree, sub[0], found, sizes, null);
            if (found.isEmpty()) throw new java.io.IOException(getString(R.string.setup_no_files));
            for (Long sz : sizes.values()) total += sz;
            long done = 0;
            byte[] buf = new byte[1 << 20];
            for (Map.Entry<String, Uri> e : found.entrySet()) {
                if (cancelImport) throw new java.io.IOException("cancelled");
                File out = new File(dir, e.getKey());
                File tmp = new File(dir, e.getKey() + ".part");
                File parent = out.getParentFile();
                //noinspection ResultOfMethodCallIgnored
                if (parent != null) parent.mkdirs();   // Movies/
                final String name = e.getKey();
                ui.post(() -> status.setText(getString(R.string.setup_copying, name)));
                try (InputStream in = cr.openInputStream(e.getValue());
                     OutputStream os = new FileOutputStream(tmp)) {
                    if (in == null) throw new java.io.IOException("cannot open " + name);
                    int n;
                    while ((n = in.read(buf)) > 0) {
                        if (cancelImport) throw new java.io.IOException("cancelled");
                        os.write(buf, 0, n);
                        done += n;
                        final int p = total > 0 ? (int) (done * 1000 / total) : 0;
                        ui.post(() -> progress.setProgress(p));
                    }
                }
                Long want = sizes.get(name);
                if (want != null && want > 0 && tmp.length() != want)
                    throw new java.io.IOException(name + ": copied " + tmp.length() + " of " + want + " bytes");
                //noinspection ResultOfMethodCallIgnored
                out.delete();
                if (!tmp.renameTo(out)) throw new java.io.IOException("cannot rename " + tmp);
                Log.i(TAG, "imported " + name + " (" + out.length() + " bytes)");
            }
            Log.i(TAG, "import done: " + found.size() + " files, " + total + " bytes");
        } catch (Exception ex) {
            Log.e(TAG, "import failed", ex);
            error = ex.getMessage();
            cleanParts(dir);   // R-161: never leave a partial file behind
        }
        if (cancelImport) return;   // the activity is gone
        final String err = error;
        final int count = found.size();
        ui.post(() -> {
            copying = false;
            progress.setVisibility(View.GONE);
            refresh();
            String head = err != null ? getString(R.string.setup_failed, err)
                                      : getString(R.string.setup_imported, count);
            status.setText(head + "\n\n" + status.getText());
            if (err == null && missing(dataDir(this), true).isEmpty()) startGame();
        });
    }

    private void startGame() {
        // NEW_TASK only: RA2Activity is singleInstance, so a running game
        // (backgrounded with Home) is brought back as it was, not restarted.
        Intent i = new Intent(this, RA2Activity.class);
        i.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        startActivity(i);
        finish();
    }
}
