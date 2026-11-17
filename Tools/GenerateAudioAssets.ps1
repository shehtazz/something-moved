param([string]$OutputDirectory = (Join-Path $PSScriptRoot '..\Media\Audio'))

New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null

Add-Type -TypeDefinition @'
using System;
using System.IO;
using System.Text;

public static class SomethingMovedAudioGenerator
{
    const int Rate = 44100;

    static void Wave(string path, int count, Func<int, double> sample)
    {
        using (var w = new BinaryWriter(File.Open(path, FileMode.Create)))
        {
            int bytes = count * 2;
            w.Write(Encoding.ASCII.GetBytes("RIFF")); w.Write(36 + bytes);
            w.Write(Encoding.ASCII.GetBytes("WAVEfmt ")); w.Write(16);
            w.Write((short)1); w.Write((short)1); w.Write(Rate);
            w.Write(Rate * 2); w.Write((short)2); w.Write((short)16);
            w.Write(Encoding.ASCII.GetBytes("data")); w.Write(bytes);
            for (int i = 0; i < count; ++i)
                w.Write((short)(Math.Max(-1.0, Math.Min(1.0, sample(i))) * 32767.0));
        }
    }

    public static void Generate(string directory)
    {
        var random = new Random(4210);
        var cycles = new int[24]; var phases = new double[24]; var levels = new double[24];
        for (int p = 0; p < cycles.Length; ++p) {
            cycles[p] = random.Next(37, 1900);
            phases[p] = random.NextDouble() * 2.0 * Math.PI;
            levels[p] = 0.6 + random.NextDouble() * 0.4;
        }
        int rainCount = Rate * 8;
        Wave(Path.Combine(directory, "rain.wav"), rainCount, i => {
            double position = (double)i / rainCount, value = 0.0;
            for (int p = 0; p < cycles.Length; ++p)
                value += Math.Sin(2.0 * Math.PI * cycles[p] * position + phases[p]) * levels[p];
            return 0.07 * value / Math.Sqrt(cycles.Length) + 0.025 * Math.Sin(2.0 * Math.PI * 56.0 * i / Rate);
        });
        random = new Random(4210);
        Wave(Path.Combine(directory, "blackout.wav"), (int)(Rate * 1.4), i => {
            double t = (double)i / Rate, e = Math.Exp(-2.6 * t);
            return e * (0.44 * Math.Sin(2.0 * Math.PI * (72.0 - 18.0 * t) * t) + 0.11 * (random.NextDouble() * 2.0 - 1.0));
        });
        Wave(Path.Combine(directory, "correct.wav"), (int)(Rate * 0.78), i => {
            double t = (double)i / Rate, e = Math.Min(1.0, t * 35.0) * Math.Exp(-3.8 * t);
            return e * (0.34 * Math.Sin(2.0 * Math.PI * 523.25 * t) + 0.23 * Math.Sin(2.0 * Math.PI * 659.25 * t));
        });
        Wave(Path.Combine(directory, "incorrect.wav"), (int)(Rate * 0.95), i => {
            double t = (double)i / Rate, e = Math.Min(1.0, t * 25.0) * Math.Exp(-2.5 * t);
            return e * (0.36 * Math.Sin(2.0 * Math.PI * 110.0 * t) + 0.30 * Math.Sin(2.0 * Math.PI * 103.0 * t));
        });
    }
}
'@

[SomethingMovedAudioGenerator]::Generate((Resolve-Path $OutputDirectory).Path)
Write-Output "Generated original mono WAV assets in $OutputDirectory"
