// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class CS_OpenSSL : ModuleRules
{
    public CS_OpenSSL(ReadOnlyTargetRules Target) : base(Target)
    {
        Type = ModuleType.External;

        string OpenSSL101sPath = Path.Combine(Target.UEThirdPartySourceDirectory, "OpenSSL", "1_0_1s");
        string OpenSSL102hPath = Path.Combine(Target.UEThirdPartySourceDirectory, "OpenSSL", "1_0_2h");
        string OpenSSL102Path = Path.Combine(Target.UEThirdPartySourceDirectory, "OpenSSL", "1.0.2g");
        string OpenSSL111Path = Path.Combine(Target.UEThirdPartySourceDirectory, "OpenSSL", "1.1.1");

        string PlatformSubdir = Target.Platform.ToString();
        string ConfigFolder = (Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT) ? "Debug" : "Release";

        if (Target.Platform == UnrealTargetPlatform.Mac)
        {
            PublicIncludePaths.Add(Path.Combine(OpenSSL102Path, "include", PlatformSubdir));

            string LibPath = Path.Combine(OpenSSL102Path, "lib", PlatformSubdir, ConfigFolder);
            //PublicLibraryPaths.Add(LibPath);
            PublicAdditionalLibraries.Add(Path.Combine(LibPath, "libssl.a"));
            PublicAdditionalLibraries.Add(Path.Combine(LibPath, "libcrypto.a"));
            PublicAdditionalLibraries.Add("z");
        }
        else if (Target.Platform == UnrealTargetPlatform.PS4)
        {
            string IncludePath = Target.UEThirdPartySourceDirectory + "OpenSSL/1.0.2g" + "/" + "include/PS4";
            string LibraryPath = Target.UEThirdPartySourceDirectory + "OpenSSL/1.0.2g" + "/" + "lib/PS4/release";
            PublicIncludePaths.Add(IncludePath);
            PublicAdditionalLibraries.Add(LibraryPath + "/" + "libssl.a");
            PublicAdditionalLibraries.Add(LibraryPath + "/" + "libcrypto.a");
        }
        else if (Target.Platform == UnrealTargetPlatform.Win64 || Target.Platform == UnrealTargetPlatform.Win32)
        {
            // Our OpenSSL 1.1.1 libraries are built with zlib compression support
            PrivateDependencyModuleNames.Add("zlib");

            // string VSVersion = "VS" + Target.WindowsPlatform.GetVisualStudioCompilerVersionName();
            string VSVersion = "VS2015";
            // Add includes
            PublicIncludePaths.Add(Path.Combine(OpenSSL111Path, "include", PlatformSubdir, VSVersion));

            // Add Libs
            string LibPath = Path.Combine(OpenSSL111Path, "lib", PlatformSubdir, VSVersion, ConfigFolder);
            PublicLibraryPaths.Add(LibPath);

            PublicAdditionalLibraries.Add(Path.Combine(LibPath, "libssl.lib"));
            PublicAdditionalLibraries.Add(Path.Combine(LibPath, "libcrypto.lib"));
            PublicAdditionalLibraries.Add("crypt32.lib");
        }
        else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Unix))
        {
            string platform = "/Linux/" + Target.Architecture;
            string IncludePath = OpenSSL102hPath + "/include" + platform;
            string LibraryPath = OpenSSL102hPath + "/lib" + platform;

            PublicIncludePaths.Add(IncludePath);
            PublicLibraryPaths.Add(LibraryPath);
            PublicAdditionalLibraries.Add(LibraryPath + "/libssl.a");
            PublicAdditionalLibraries.Add(LibraryPath + "/libcrypto.a");

            PublicDependencyModuleNames.Add("zlib");
            //			PublicAdditionalLibraries.Add("z");
        }
        else if (Target.Platform == UnrealTargetPlatform.Android)
        {
            string IncludePath = OpenSSL101sPath + "/include/Android";
            PublicIncludePaths.Add(IncludePath);

            // unneeded since included in libcurl
            // string LibPath = Path.Combine(OpenSSL101sPath, "lib", PlatformSubdir);
            //PublicLibraryPaths.Add(LibPath);
        }
        else if (Target.Platform == UnrealTargetPlatform.IOS)
        {
            string IncludePath = OpenSSL101sPath + "/include/IOS";
            string LibraryPath = OpenSSL101sPath + "/lib/IOS";

            PublicIncludePaths.Add(IncludePath);

            PublicAdditionalLibraries.Add(LibraryPath + "/libssl.a");
            PublicAdditionalLibraries.Add(LibraryPath + "/libcrypto.a");
        }

        PublicIncludePaths.Add(Path.Combine(ModuleDirectory,"Source"));

    }
}
