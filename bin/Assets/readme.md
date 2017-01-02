This folder contains useful SubVi's. It is part of an (ongoing) attempt to make the Beadtracker more readable/manageable.

A list of files + description follows.

# ReadDefaultConfig.vi

This reads the file default-config.xml, which is expected to be in the root directory of the tracker. (i.e. bin/).
It also accepts a FILE input and outputs (MeasurementConfig config, Path path, Error error out);

# UpdateCameraSettings.vi

This unbundles the MeasurementConfig stream and sets the given camera to the proper settings.