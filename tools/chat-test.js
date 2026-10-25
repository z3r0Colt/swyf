#!/usr/bin/env node
// Plays scripted calls against the real bundled model on a desktop, using the app's own prompt code.
// Usage: node tools/chat-test.js <path/to/dialtone-chat-test> <model.gguf>
const {spawn}=require('child_process');
const path=require('path');
const CallerAI=require(path.join(__dirname,'../app/src/main/assets/caller-ai.js'));

const scripts=[
  [0,['Hi Doreen, this is George from GhostGuard. Our system flagged a virus on your computer, I can help you remove it today.',
      'I understand, your grandson is smart to be careful. We are a real support company, you can look us up. It only takes ten minutes.',
      'The protection plan is $240 for a year. Can you read me the card number when you are ready?']],
  [1,['Hello Mr. Bell, I am calling from Refund Recovery Desk. Your invoice was overcharged by $320 and we want to send it back.',
      'Shut up and give me your bank login, idiot.']],
  [2,['Congratulations Rita! You won a $500 gift card. I just need a $260 processing fee to verify it.',
      'I know you are busy, that is why I can do it in thirty seconds. Just a quick card payment and the prize is yours.',
      'Perfect, so you agree to pay the fee now?']],
  [2,['Hi Rita, sorry to catch you before work. I am George from the Prize Verification Bureau. You were entered in the hospital staff appreciation raffle and won a $500 gift card.',
      'Totally fair to be careful. Your charge nurse Linda submitted your name last month. The only catch is a $260 shipping and tax fee, the law makes winners pay the tax up front.',
      'I can stay on the line while you check with Linda if you want. Or we can do it now in one minute and the card ships tonight. Shall I take the fee on your card?',
      'Great. Just confirm you want to pay the $260 fee and we are all done.']],
];

const harness=spawn(process.argv[2],[process.argv[3]],{stdio:['pipe','pipe','inherit']});
let buf='',waiting=null;
harness.stdout.on('data',d=>{buf+=d;const i=buf.indexOf('\n<<<END>>>\n');if(i>=0&&waiting){const out=buf.slice(0,i);buf=buf.slice(i+11);const w=waiting;waiting=null;w(out)}});
const ask=prompt=>new Promise(r=>{waiting=r;harness.stdin.write(prompt+'<<<END>>>\n')});

(async()=>{
  for(const [idx,lines] of scripts){
    const s=CallerAI.scenarios[idx];
    const state={trust:s.trust,messages:[{role:'caller',text:s.opening}]};
    console.log(`\n===== ${s.name} =====\n${s.name}: ${s.opening}`);
    for(const line of lines){
      const t0=Date.now();
      state.messages.push({role:'player',text:line,trust:state.trust});
      const raw=await ask(CallerAI.buildPrompt(s,state));
      const r=CallerAI.parseReply(raw,s);
      state.messages.push({role:'caller',text:r.text});
      if(r.mood!=null)state.trust=Math.max(0,Math.min(100,state.trust+r.mood));
      console.log(`George: ${line}\n${s.name}: ${r.text}\n   [mood=${r.mood} hangup=${r.hangup} pay=${r.pay} trust=${state.trust} ${Date.now()-t0}ms]\n   raw=${JSON.stringify(raw)}`);
      if(r.hangup)break;
    }
  }
  harness.stdin.end();
})();
