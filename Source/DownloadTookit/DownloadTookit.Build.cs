// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;
public class DownloadTookit : ModuleRules
{
	public DownloadTookit(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		LoadOpenSSLLib(Target);
		
		PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...

			}
			);
				
		
		PrivateIncludePaths.AddRange(
			new string[] {
				// ... add other private include paths required here ...
			}
			);
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
                "HTTP",
                "SSL"
				// ... add other public dependencies that you statically link with here ...
			}
			);
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				// ... add private dependencies that you statically link with here ...	
			}
			);
		
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);

        PublicDefinitions.AddRange(new string[]{
            "WITH_LOG=0",
            "HACK_HTTP_LOG_GETCONTENT_WARNING=1"
        });
	
        OptimizeCode = CodeOptimization.InShippingBuildsOnly;
    }
	
	public void LoadOpenSSLLib(ReadOnlyTargetRules Target)
	{
		string OpenSSL101sPath = Path.Combine(Target.UEThirdPartySourceDirectory, "OpenSSL", "1_0_1s");
        string OpenSSL102hPath = Path.Combine(Target.UEThirdPartySourceDirectory, "OpenSSL", "1_0_2h");
        string OpenSSL102Path = Path.Combine(Target.UEThirdPartySourceDirectory, "OpenSSL", "1.0.2g");
        string OpenSSL111Path = Path.Combine(Target.UEThirdPartySourceDirectory, "OpenSSL", "1.1.1");
        string OpenSSL111kPath = Path.Combine(Target.UEThirdPartySourceDirectory, "OpenSSL", "1.1.1k");

        string PlatformSubdir = Target.Platform.ToString();
        string ConfigFolder = (Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT) ? "Debug" : "Release";

        if (Target.Platform == UnrealTargetPlatform.Mac)
        {
            PublicIncludePaths.Add(Path.Combine(OpenSSL111kPath, "include", PlatformSubdir));

            string LibPath = Path.Combine(OpenSSL111kPath, "lib", PlatformSubdir, ConfigFolder);
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
            PublicIncludePaths.Add(Path.Combine(OpenSSL111kPath, "include", PlatformSubdir, VSVersion));

            // Add Libs
            string LibPath = Path.Combine(OpenSSL111kPath, "lib", PlatformSubdir, VSVersion, ConfigFolder);
            PublicLibraryPaths.Add(LibPath);

            PublicAdditionalLibraries.Add(Path.Combine(LibPath, "libssl.lib"));
            PublicAdditionalLibraries.Add(Path.Combine(LibPath, "libcrypto.lib"));
            PublicSystemLibraries.Add("crypt32.lib");
        }
        else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Unix))
        {
            string platform = "/Linux/" + Target.Architecture;
            string IncludePath = OpenSSL111kPath + "/include" + platform;
            string LibraryPath = OpenSSL111kPath + "/lib" + platform;

            PublicIncludePaths.Add(IncludePath);
            PublicLibraryPaths.Add(LibraryPath);
            PublicAdditionalLibraries.Add(LibraryPath + "/libssl.a");
            PublicAdditionalLibraries.Add(LibraryPath + "/libcrypto.a");

            PublicDependencyModuleNames.Add("zlib");
            //			PublicAdditionalLibraries.Add("z");
        }
        else if (Target.Platform == UnrealTargetPlatform.Android)
        {
            string IncludePath = OpenSSL111kPath + "/include/Android";
            PublicIncludePaths.Add(IncludePath);

            // unneeded since included in libcurl
            // string LibPath = Path.Combine(OpenSSL111kPath, "lib", PlatformSubdir);
            //PublicLibraryPaths.Add(LibPath);
        }
        else if (Target.Platform == UnrealTargetPlatform.IOS)
        {
            string IncludePath = OpenSSL111kPath + "/include/IOS";
            string LibraryPath = OpenSSL111kPath + "/lib/IOS";

            PublicIncludePaths.Add(IncludePath);

            PublicAdditionalLibraries.Add(LibraryPath + "/libssl.a");
            PublicAdditionalLibraries.Add(LibraryPath + "/libcrypto.a");
        }

        PublicIncludePaths.Add(Path.Combine(ModuleDirectory,"Source"));

    }
}
