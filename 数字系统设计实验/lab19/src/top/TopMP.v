`timescale 1ns / 1ps

//top_music_player
module top_music_player (
    input  wire clk,
    input  wire reset_btn,

    //未消抖的原始输入
    input  wire btn_play_in,
    input  wire btn_next_in,

    // 音频编解码器
    input  wire ADC_SDATA,
    output wire DAC_SDATA,
    output wire BCLK,
    output wire MCLK,
    output wire LRCLK,
    inout  wire SCL,
    inout  wire SDA,

    // LED 状态指示
    output wire        led_play,
    output wire [1:0]  led_song
);

    wire sys_clk;       // 系统逻辑时钟
    wire audio_clk;     // 由 DCM 生成的音频时钟
    wire locked;        // DCM 锁定信号
    wire sys_reset;     // 复位

    wire btn_play_pulse;
    wire btn_next_pulse;
    wire NewFrame;
    wire [15:0] sample;


    // 1. 实例化DCM
    sys_dcm u_dcm (
        .clk_in1  (clk),
        .reset    (reset_btn),
        .clk_out1 (sys_clk),
        .clk_out2 (audio_clk),
        .locked   (locked)
    );

    // 只有锁相环稳定了才撤销复位
    assign sys_reset = reset_btn | (~locked);

    // 2. 实例化按键处理模块 (接 sys_clk)
    button_process_unit u_btn_play (
        .clk       (sys_clk),    // 使用 sys_clk
        .reset     (sys_reset),
        .ButtonIn  (btn_play_in),
        .ButtonOut (btn_play_pulse)
    );

    button_process_unit u_btn_next (
        .clk       (sys_clk),
        .reset     (sys_reset),
        .ButtonIn  (btn_next_in),
        .ButtonOut (btn_next_pulse)
    );

    // 3. 实例化音频编解码接口 (接 audio_clk)
    // 低位填充
    wire [23:0] play_data = {sample, 8'd0}; 

    AudioInterface u_audio_interface (
        .clk           (audio_clk),  //接 audio_clk
        .reset         (sys_reset),
        .ADC_SDATA     (ADC_SDATA),
        .DAC_SDATA     (DAC_SDATA),
        .BCLK          (BCLK),
        .MCLK          (MCLK),
        .LRCLK         (LRCLK),
        .SCL           (SCL),
        .SDA           (SDA),
        .error         (),
        .LeftPlayData  (play_data),  
        .RightPlayData (play_data),  
        .LeftRecData   (),           
        .RightRecData  (),           
        .NewFrame      (NewFrame)    
    );

    // 4. 实例化次顶层
    music_player #(.sim(0)) u_music_player (
        .clk        (sys_clk),       // 使用 sys_clk
        .reset      (sys_reset),
        .play_pause (btn_play_pulse),
        .next       (btn_next_pulse),
        .NewFrame   (NewFrame),
        
        .sample     (sample),         
        .play       (led_play),
        .song       (led_song)
    );

endmodule