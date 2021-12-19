export class HDLPlayer {
    private _video: HTMLVideoElement = document.createElement('video');

    init (url: string): void {
        const source = document.createElement('source');
        source.src = url;
        this._video.appendChild(source);
        this._video.load();
    }

    play () {
        const promise = this._video.play();
    }

    getTexImageSource ():TexImageSource | null {
        this._video.width = this._video.videoWidth;
        this._video.height = this._video.videoHeight;
        return this._video;
    }
}
