hello! I would like to create a utility mod for 1.21.11 it will be used in single player only.
The client is made fully in c++, with the menu being imgui and injection handled by minhook. Any modules are implemented via JNI with a consistent logic that will not break.
DO NOT assume this is going to be updated. If i say a specific version like fabric 1.21.11 you do not need to care if it breaks on next update, we are not going to be updating.
I also kindly advise please use Vanilla packets, do not make custom packets that will break vanilla or send invalidated packets to the server.