## rMirror

rMirror is a simple backup utility that creates a zip archive of a directory.

## Configuration

rMirror will look at the root of the path to be backed up for a `.rmirror` file
containing configuration options for the backup job.

Configuration is done through `name=value` lines.

If no `.rmirror` file is found then a default configuration is used.

### Configuration options
```
# comment
append_date=[true|false]
append_time=[true|false]
backups_to_keep=[integer]
file_name=[string]
ignore_dirs=[comma separated string]
ignore_file_types=[comma separated string]
ignore_files=[comma separated string]
move_to=[string path]
overwrite_existing_backup=[true|false]
```

