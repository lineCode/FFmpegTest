using System;
using System.Runtime.InteropServices;

namespace ConsoleApplication1
{
    public static class RtspWriteToFile
    {
        /// <summary>
        /// Путь к библиотеке с модулем записи
        /// </summary>
        private const string RTSP_RECORDER_DLL_NAME = @"\Dll\RtspWriteToFile.dll";

        /// <summary>
        /// Сигнатура функции обратного вызова получения фрейма
        /// </summary>
        /// <param name="buffer">указатель на буфер</param>
        /// <param name="bufferSize">размер буфера</param>
        public delegate void GetImageCallback(IntPtr buffer, int bufferSize);

        /// <summary>
        /// Старт получения данных по rtsp
        /// </summary>
        /// <param name="rtsp">ртсп ссылка</param>
        /// <param name="outputFile">выходной файл</param>
        /// <returns>Идентификатор сессии или IntPtr.Zero если ошибка</returns>
        [DllImport(RTSP_RECORDER_DLL_NAME)]
        public static extern IntPtr Record(string rtsp, string outputFile);


        /// <summary>
        /// Старт получения данных по rtsp
        /// </summary>
        /// <param name="rtsp">ртсп ссылка</param>
        /// <param name="getImageCallback">выходной поток</param>
        /// <returns>Идентификатор сессии или IntPtr.Zero если ошибка</returns>
        [DllImport(RTSP_RECORDER_DLL_NAME)]
        public static extern IntPtr Online(string rtsp, GetImageCallback getImageCallback);
    }
}