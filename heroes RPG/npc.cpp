
void merchantNPC(int itemID[],string itemName[],int itemCost[],int itemAttr[][3], int inventoryID[],int inventoryQTY[],int &myGold,int hp){
    if(hp==0){
        cout<<"I have no business with you!";
        Sleep(2000);
        return;
    }
    while(1){
        cls;
        showTitle();
        showCatalogue(itemID,itemName,itemCost,itemAttr);
        showBag(inventoryID,inventoryQTY,itemID,itemName,itemAttr);
        cout<<"Gold: "<<myGold<<'\n';

        char ch;
        cout<<"(B)uy (S)ale (A)uction or e(X)it?: "; cin>>ch; ch=toupper(ch);
        if(ch=='X') break;

        int id;
        cout<<"Item ID#:";cin>>id;//item for transaction

        bool forSale=1;
        for(int i=0;i<20;i++){
            if(itemID[i]==id and itemCost[i]==0){
                forSale=0;// item not for sale
                break;
            }
        }
        if(!forSale and ch!='A') {cout<<"Item is NOT FOR SALE";Sleep(2000);continue;}

        bool inCatalogue=0;
        for(int i=0;i<20;i++){
            if(itemID[i]==id) inCatalogue=1;//found in catalogue
        }
        if(!inCatalogue) {cout<<"Invalid ID";Sleep(2000);continue;}

        bool inBag=0;//exist in bag
        int index;
        for(int i=0;i<10;i++){//search bag
            if(inventoryID[i]==id) {inBag=1;index=i;}//if found, index will have valid value
        }
        if(ch=='S' and !inBag) {cout<<"No such item in bag";Sleep(2000);continue;}//sale item not in bag

        int trxQty;//transaction qty
        if(ch!='A'){
            cout<<"How many? ";
            cin>>trxQty;
        }

        int cost;
        for(int i=0;i<20;i++){//cek harga
            if(itemID[i]==id) {
                cost=itemCost[i];
                break;
            }
        }

        int bagQty=0;//if 10 bag full
        for(int i=0;i<10;i++){
            if(inventoryQTY[i]!=0) bagQty++;
        }
        if(ch=='B') {
            if(bagQty==10 and !inBag){cout<<"Bag is full...";Sleep(2000);continue;}//can not buy new item while bag full

            if(myGold<trxQty*cost) {cout<<"\nNot enough gold";Sleep(2000);continue;}//not enough gold

            if(inBag) {//if inbag, update
                inventoryQTY[index]+=trxQty;
                myGold-=cost*trxQty;
            }
            else{//if not inbag add new elemen
                for(int i=0;i<10;i++){
                    if(inventoryQTY[i]==0){
                        inventoryID[i]=id;
                        inventoryQTY[i]=trxQty;
                        break;
                    }
                }
                myGold-=cost*trxQty;
            }
        }
        else if(ch=='S'){
            if(inventoryQTY[index]<trxQty) {cout<<"\nItem in bag is "<<inventoryQTY[index];Sleep(2000);continue;}
            inventoryQTY[index]-=trxQty;
            if(inventoryQTY[index]==0) inventoryID[index]=0;
            myGold+=cost*trxQty;
        }
        else if(ch=='A'){
            int aucPrice;
            cout<<"Put Minimum Prize for the item : ";
            cin>>aucPrice;
            cout<<"Prepare an auction for "<<id2Name(id,itemID,itemName)<<endl;
            showBar(500);
            int rndAuction=rand()%10;
            if(rndAuction==0){
                cout<<"Auction failed... maybe the price is too high, please reconsider...";
                Sleep(5000);
            }
            int aucResult=aucPrice;
            for(int i=0;i<rndAuction;i++){
                int bidIn=rand()%3;
                if(bidIn==0) break;
                aucResult+=aucPrice/10;
                cout<<"\nNext bid open and accepted at "<<aucResult;
                Sleep(3000);
            }
            cout<<"\nCongratulation, Last bid accepted at "<<aucResult<< endl;
            cout<<"Merchant fee is 10%...Hahaha "<< endl;
            Sleep(5000);
            aucResult*=0.9;
            myGold+=aucResult;
            updBag(id,inventoryID,inventoryQTY,-1);
        }

    }
}
void changeWeapon(int &weaponID,int &weaponUP,int itemID[],string itemName[],int itemAttr[][3],int inventoryID[],int inventoryQTY[]){

    while(1){
        cls;
        showTitle();
        cout<<"CHANGE WEAPONRY\n";
        showBag(inventoryID,inventoryQTY,itemID,itemName,itemAttr);

        cout<<"\nMy Equipment"<<'\n';
        cout<<"WEAPON : ";
        if(weaponID!=0) {
            showAttr(weaponID,itemID,itemAttr);
            cout<<' ';
            showStr(id2Name(weaponID,itemID,itemName),15);
            cout<<" Lv"<<weaponUP;
        }
        else cout<<"No Weapon held";
        cout<<"\n\n";

        if(!yesno("Change Weapon")) break;
        if(weaponUP>1){
            if(!yesno("You will lose the Upgraded Weapon, are you sure")) break;
        }

        int equipID;
        cout<<"\nEnter ID : ";
        cin>>equipID;
        if(equipID/10!=1) {cout<<"Choose a Weapon";Sleep(2000);continue;}
        if(weaponID==equipID) {cout<<"This Weapon already equipped";Sleep(2000);continue;}
        if(!cekID(equipID,inventoryID,10)==-1) {cout<<"Invalid ID";Sleep(2000);continue;} //cek apakah id ada di bag

        if(weaponID==0) {
            for(int i=0;i<10;i++){ //update slot weapon baru qty -1
                if(inventoryID[i]==equipID){
                    weaponID=equipID; //recently lose weapon in battle
                    inventoryQTY[i]--;
                    if(inventoryQTY[i]==0) inventoryID[i]=0;
                }
            }
        }
        else{ //normal equipment change
            bool fullBag=1;
            int xweaponID=weaponID;
            for(int i=0;i<10;i++){
                if(inventoryID[i]==weaponID) {fullBag=0;break;}
                if(inventoryID[i]==equipID and inventoryQTY[i]==1) {fullBag=0;break;}
                if(inventoryID[i]==0) {fullBag=0;break;}
            }
            if(fullBag) {cout<<"Inventory full";Sleep(2000);continue;}

            if(weaponUP>1){//upgraded weapon will lose its upgrade value
                weaponUP=1;
            }
            for(int i=0;i<10;i++){ //update slot weapon baru qty -1
                if(inventoryID[i]==equipID){
                    weaponID=equipID;
                    inventoryQTY[i]--;
                    if(inventoryQTY[i]==0) inventoryID[i]=0;
                }
            }
            for(int i=0;i<10;i++){ //update slot weapon lama qty +1
                if(inventoryID[i]==xweaponID){
                    inventoryQTY[i]++;
                    break;
                }
                if(inventoryID[i]==0 and cekID(xweaponID,inventoryID,10)==-1){
                    inventoryID[i]=xweaponID;
                    inventoryQTY[i]++;
                    break;
                }
            }
        }
    }
}
void changeArmor(int &armorID,int &armorUP,int itemID[],string itemName[],int itemAttr[][3],int inventoryID[],int inventoryQTY[]){

    while(1){
        cls;
        showTitle();
        cout<<"CHANGE ARMORY\n";
        showBag(inventoryID,inventoryQTY,itemID,itemName,itemAttr);

        cout<<"\nMy Equipment"<<'\n';
        cout<<"ARMOR : ";
        if(armorID!=0) {
            showAttr(armorID,itemID,itemAttr);
            cout<<' ';
            showStr(id2Name(armorID,itemID,itemName),15);
            cout<<" Lv"<<armorUP;
        }
        else cout<<"No Armor held";
        cout<<"\n\n";

        if(!yesno("Change Armor")) break;
        if(armorUP>1){
            if(!yesno("You will lose the Upgraded Armor, are you sure")) break;
        }

        int equipID;
        cout<<"\nEnter ID : ";
        cin>>equipID;
        if(equipID/10!=2) {cout<<"Choose a Armor";Sleep(2000);continue;}
        if(armorID==equipID) {cout<<"This Armor already equipped";Sleep(2000);continue;}
        if(cekID(equipID,inventoryID,10)==-1) {cout<<"Invalid ID";Sleep(2000);continue;} //cek apakah id ada di bag

        if(armorID==0) {
            for(int i=0;i<10;i++){ //update slot armor baru qty -1
                if(inventoryID[i]==equipID){
                    armorID=equipID; //recently lose armor in battle
                    inventoryQTY[i]--;
                    if(inventoryQTY[i]==0) inventoryID[i]=0;
                }
            }
        }
        else{ //normal equipment change
            bool fullBag=1;
            int xarmorID=armorID;
            for(int i=0;i<10;i++){
                if(inventoryID[i]==armorID) {fullBag=0;break;}
                if(inventoryID[i]==equipID and inventoryQTY[i]==1) {fullBag=0;break;}
                if(inventoryID[i]==0) {fullBag=0;break;}
            }
            if(fullBag) {cout<<"Inventory full";Sleep(2000);continue;}

            if(armorUP>1){//upgraded armor will lose its upgrade value
                armorUP=1;
            }
            for(int i=0;i<10;i++){ //update slot armor baru qty -1
                if(inventoryID[i]==equipID){
                    armorID=equipID;
                    inventoryQTY[i]--;
                    if(inventoryQTY[i]==0) inventoryID[i]=0;
                }
            }
            for(int i=0;i<10;i++){ //update slot armor lama qty +1
                if(inventoryID[i]==xarmorID){
                    inventoryQTY[i]++;
                    break;
                }
                if(inventoryID[i]==0 and cekID(xarmorID,inventoryID,10)==-1){
                    inventoryID[i]=xarmorID;
                    inventoryQTY[i]++;
                    break;
                }
            }
        }
    }
}
void upgWeapon(int weaponID,int &weaponUP,int &exp,int &gold,int itemID[],string itemName[],int itemAttr[][3],int inventoryID[],int inventoryQTY[]){

    while(1){
        cls;
        showTitle();
        cout<<"UPGRADE WEAPONRY\n";
        showBag(inventoryID,inventoryQTY,itemID,itemName,itemAttr);

        cout<<"\nMy Equipment"<<'\n';
        cout<<"WEAPON : ";cout<<id2Name(weaponID,itemID,itemName)<<" Lv"<<weaponUP;
        showAttr(weaponID,itemID,itemAttr);
        cout<<"\n\n";

        cout<<"a Flaming dust, some experience, and a bag of gold"<<'\n';
        if(!yesno("Upgrade Weapon")) break;

        bool isItem=0,isExp=0,isGold=0;
        int idx;
        for(int i=0;i<10;i++){
            if(inventoryID[i]==42 and inventoryQTY[i]>=weaponUP) {idx=i;isItem=1;break;}
        }
        if(!isItem) {cout<<"Go Back Home";Sleep(2000);break;} //cek apakah seed ada di bag

        if(exp>=weaponUP*1000) isExp=1;
        if(!isExp) {cout<<"Hunt some more";Sleep(2000);break;} //cek apakah cukup exp

        if(gold>=weaponUP*10000) isGold=1;
        if(!isGold) {cout<<".....";Sleep(2000);break;} //cek apakah cukup gold

        inventoryQTY[idx]--;
        if(inventoryQTY[idx]==0) inventoryID[idx]=0;

        exp-=weaponUP*1000;
        gold-=weaponUP*10000;
        weaponUP++;
        showBar(200);
        cout<<"\nYou feel more Powerful";Sleep(3000);break;

    }
}

void upgArmor(int armorID,int &armorUP,int &exp,int &gold,int itemID[],string itemName[],int itemAttr[][3],int inventoryID[],int inventoryQTY[]){

    while(1){
        cls;
        showTitle();
        cout<<"UPGRADE ARMORY\n";
        showBag(inventoryID,inventoryQTY,itemID,itemName,itemAttr);

        cout<<"\nMy Equipment"<<'\n';
        cout<<"ARMOR : ";cout<<id2Name(armorID,itemID,itemName)<<" Lv"<<armorUP;
        showAttr(armorID,itemID,itemAttr);
        cout<<"\n\n";

        cout<<"Ancient sea creature, some experience, and a bag of gold"<<'\n';
        if(!yesno("Upgrade Armor")) break;

        bool isItem=0,isExp=0,isGold=0;
        int idx;
        for(int i=0;i<10;i++){
            if(inventoryID[i]==42 and inventoryQTY[i]>=armorUP) {idx=i;isItem=1;break;}
        }
        if(!isItem) {cout<<"Go Back Home";Sleep(2000);break;} //cek apakah seed ada di bag

        if(exp>=armorUP*1000) isExp=1;
        if(!isExp) {cout<<"Hunt some more";Sleep(2000);break;} //cek apakah cukup exp

        if(gold>=armorUP*10000) isGold=1;
        if(!isGold) {cout<<".....";Sleep(2000);break;} //cek apakah cukup gold

        inventoryQTY[idx]--;
        if(inventoryQTY[idx]==0) inventoryID[idx]=0;

        exp-=armorUP*1000;
        gold-=armorUP*10000;
        armorUP++;
        showBar(200);
        cout<<"\nYou feel more Safe than ever";Sleep(3000);break;

    }
}

void forgeWeapon(int itemID[],string itemName[],int itemAttr[][3],int inventoryID[],int inventoryQTY[],int recipe1ID,int recipe1Item[],int &forgeLvl){
//    cout<<"c"<<sizeof(recipe1Item)/sizeof(recipe1Item[0]);
//    for(int i=0;i<3;i++) cout<<recipe1Item[i]<<'\n';
//    system("pause");

    if(!yesno("Forge Weapon")) return;
    cout<<"You need to sacrifice 3 item to forge The Powerful Artifact\n";
    cout<<"Also there's chance you might fail...\n";
    cout<<"WARNING:Make sure you have empty slot in your bag\n";
    if(!yesno("Are you sure")) return;

    int forgedItem[3],forgedID; //array hold the craft item
    bool cancelForge=0;
    for(int i=0;i<3;i++){
        cls;
        showTitle();
        cout<<"FORGING ARTIFACT\n";
        showBag(inventoryID,inventoryQTY,itemID,itemName,itemAttr);
        cout<<"\n\n";

        if(!yesno("Proceed")){
            cout<<"You have canceled Forging...";
            cancelForge=1;
            Sleep(3000);
            break;
        }
        cout<<"Submit Item : ";
        cin>>forgedID;
        if(cekID(forgedID,inventoryID,10)==-1) {cout<<"Invalid ID!";Sleep(2000);i--;continue;}
        forgedItem[i]=forgedID; //id store into array
    }
    if(cancelForge) return;

    bool matchedForge=1; //if craft item list identical to the recipe list
    for(int i=0;i<3;i++){
        if(forgedItem[i]!=recipe1Item[i]){
            matchedForge=0;
        }
        updBag(forgedItem[i],inventoryID,inventoryQTY,-1); //update bag (items dissolve)
    }
    if(!matchedForge){
        errMsg("Submitted item don't match with recipe");
        return;
    }

    int forgeChance=rand()%10; //randomize chance of success base on lvl of forging (0 is successful)
    if(forgeChance<forgeLvl){
        if(forgeLvl<10) forgeLvl++; //increase forging lvl (max at 10)
        updBag(recipe1ID,inventoryID,inventoryQTY,1);
        cout<<"Forging SUCCESS!!! You now possess..."<<id2Name(recipe1ID,itemID,itemName);
        Sleep(3000);
    }
    else{
        cout<<"Forging FAILED, try again later...";
        Sleep(3000);
    }
}
void forgeArmor(int itemID[],string itemName[],int itemAttr[][3],int inventoryID[],int inventoryQTY[],int recipe2ID,int recipe2Item[],int &forgeLvl){

    if(!yesno("Forge Armor")) return;
    cout<<"You need to sacrifice 4 item to forge The Powerful Artifact\n";
    cout<<"Also there's chance you might fail...\n";
    cout<<"WARNING:Make sure you have empty slot in your bag\n";
    if(!yesno("Are you sure")) return;

    int forgedItem[4],forgedID; //array hold the craft item
    bool cancelForge=0;
    for(int i=0;i<4;i++){
        cls;
        showTitle();
        cout<<"FORGING ARTIFACT\n";
        showBag(inventoryID,inventoryQTY,itemID,itemName,itemAttr);
        cout<<"\n\n";

        if(!yesno("Proceed")){
            cout<<"You have canceled Forging...";
            cancelForge=1;
            Sleep(3000);
            break;
        }
        cout<<"Submit Item : ";
        cin>>forgedID;
        if(cekID(forgedID,inventoryID,10)==-1) {cout<<"Invalid ID!";Sleep(2000);i--;continue;}
        forgedItem[i]=forgedID; //id store into array
    }
    if(cancelForge) return;

    bool matchedForge=1; //if craft item list identical to the recipe list
    for(int i=0;i<4;i++){
        if(forgedItem[i]!=recipe2Item[i]){
            matchedForge=0;
        }
        updBag(forgedItem[i],inventoryID,inventoryQTY,-1); //update bag (items dissolve)
    }
    if(matchedForge and forgeLvl<10) forgeLvl++; //increase forging lvl (max at 10)
    int forgeChance=rand()%(11-forgeLvl); //randomize chance of success base on lvl of forging (0 is successful)
    if(forgeChance==0){
        updBag(recipe2ID,inventoryID,inventoryQTY,1);
        cout<<"Forging SUCCESS!!! You now possess..."<<id2Name(recipe2ID,itemID,itemName);
        Sleep(3000);
    }
    else{
        cout<<"Forging FAILED, try again later...";
        Sleep(3000);
    }
}

void blacksmithNPC(string username,int maxmp,int mp,int maxhp,int hp,int dmg,int def,int life,int &gold,int lv,int &exp,int tnl,int path,int &weaponID,int &armorID,int &weaponUP,int &armorUP,int itemID[],string itemName[],int itemAttr[][3], int inventoryID[],int inventoryQTY[],int recipe1ID,int recipe1Item[],int recipe2ID,int recipe2Item[],int &forgeLvl){
    if(hp==0){
        cout<<"Empty soul, you don't belong here";
        Sleep(2000);
        return;
    }

    int menuBS;
    string equipMenu[3]={"Weapon","Armor","EXIT"};
    string bsMenu[4]={"Change Equipment","Upgrade Equipment","Forge Equipment","EXIT"};
    while(1){
        cls;
        showTitle();
        string weaponName=id2Name(weaponID,itemID,itemName);
        string armorName=id2Name(armorID,itemID,itemName);
        heroStat2(username,maxmp,mp,maxhp,hp,dmg,def,life,lv,exp,tnl,path,gold,weaponID,armorID,weaponUP,armorUP,itemID,itemName,itemAttr);
        cout<<"BLACKSMITH TENT"<<'\n';
        menuBS=menu(bsMenu,4);

        if(menuBS==1){
            int menuBS1=menu(equipMenu,3);
            if(menuBS1==1) changeWeapon(weaponID,weaponUP,itemID,itemName,itemAttr,inventoryID,inventoryQTY);
            else if(menuBS1==2) changeArmor(armorID,armorUP,itemID,itemName,itemAttr,inventoryID,inventoryQTY);
            else if(menuBS1==3) continue;
            else {cout<<"Choose 1-3...";Sleep(2000);continue;}
        }
        else if(menuBS==2){
            int menuBS2=menu(equipMenu,3);
            if(menuBS2==1) upgWeapon(weaponID,weaponUP,exp,gold,itemID,itemName,itemAttr,inventoryID,inventoryQTY);
            else if(menuBS2==2) upgArmor(armorID,armorUP,exp,gold,itemID,itemName,itemAttr,inventoryID,inventoryQTY);
            else if(menuBS2==3) continue;
            else {cout<<"Choose 1-3...";Sleep(2000);continue;}
        }
        else if(menuBS==3){
            int menuBS3=menu(equipMenu,3);;
            if(menuBS3==1) forgeWeapon(itemID,itemName,itemAttr,inventoryID,inventoryQTY,recipe1ID,recipe1Item,forgeLvl);
            else if(menuBS3==2) forgeArmor(itemID,itemName,itemAttr,inventoryID,inventoryQTY,recipe2ID,recipe2Item,forgeLvl);
            else if(menuBS3==3) continue;
            else {cout<<"Choose 1-3...";Sleep(2000);continue;}
        }
        else if(menuBS==4) break;
        else {cout<<"Choose 1-4...";Sleep(2000);continue;}
    }
}
void addHP(string heroName,int heroLVL,int &heroMaxHP,int &heroEXP,int heroPath,int &heroGold){
    if(!yesno("Increase Maximum Health Point")) return;
    else {
        if(!yesno("Once you give me your Exp, you can't take it back, are you sure")) return;
    }

    int sacrificeExp;
    cout<<"How many Exp you want to Sacrifice?";
    cin>>sacrificeExp;
    if(sacrificeExp>heroEXP){
        cout<<"You don't have that much EXP!!";
        Sleep(3000);
        return;
    }

    int points=sacrificeExp/50;
    if(points<1) {
        heroEXP-=sacrificeExp;
        cout<<"Not enough sacrifices, come again when you're ready";
        Sleep(3000);
        return;
    }
    if(heroGold<points*100){
        cout<<"Not enough Gold! Go home";
        Sleep(3000);
        return;
    }
    showBar(200);
    heroMaxHP+=points*10*heroPath/heroLVL;
    heroEXP-=sacrificeExp;
    heroGold-=points*100;
}
void addMP(string heroName,int heroLVL,int &heroMaxMP,int &heroEXP,int heroPath,int &heroGold){
    if(!yesno("Increase Maximum Manna Point")) return;
    else {
        if(!yesno("Once you give me your Exp, you can't take it back, are you sure")) return;
    }

    int sacrificeExp;
    cout<<"How many Exp you want to Sacrifice?";
    cin>>sacrificeExp;
    if(sacrificeExp>heroEXP){
        cout<<"You don't have that much EXP!!";
        Sleep(3000);
        return;
    }
    int points=sacrificeExp/250;
    if(points<1) {
        heroEXP-=sacrificeExp;
        cout<<"Not enough sacrifices, come again when you're ready";
        Sleep(3000);
        return;
    }
    if(heroGold<points*100){
        cout<<"Not enough Gold! Go home";
        Sleep(3000);
        return;
    }
    showBar(200);
    heroMaxMP+=points*10*heroPath/heroLVL;
    heroEXP-=sacrificeExp;
    heroGold-=points*100;
}

void addDamage(string heroName,int heroLVL,int &heroDMG,int &heroEXP,int heroPath,int &heroGold){
    if(!yesno("Build Damage")) return;
    if(!yesno("Once you give me your Exp, you can't take it back, are you sure")) return;

    int sacrificeExp;
    cout<<"How many Exp you want to Sacrifice?";
    cin>>sacrificeExp;
    if(sacrificeExp>heroEXP){
        cout<<"You don't have that much EXP!!";
        Sleep(3000);
        return;
    }
    if(sacrificeExp>heroEXP) warningMsg("You don't have that much Exp");
    int points=sacrificeExp/100;
    if(points<1) {
        heroEXP-=sacrificeExp;
        cout<<"Not enough sacrifices, come again when you're ready";
        Sleep(3000);
        return;
    }
    if(heroGold<points*100){
        cout<<"Not enough Gold! Go home";
        Sleep(3000);
        return;
    }
    showBar(200);
    heroDMG+=points*10*heroPath/heroLVL;
    heroEXP-=sacrificeExp;
    heroGold-=points*100;
}
void addArmor(string heroName,int heroLVL,int &heroDEF,int &heroEXP,int heroPath,int &heroGold){
    if(!yesno("Build Armor")) return;
    else {
        if(!yesno("Once you give me your Exp, you can't take it back, are you sure")) return;
    }

    int sacrificeExp;
    cout<<"How many Exp you want to Sacrifice?";
    cin>>sacrificeExp;
    if(sacrificeExp>heroEXP){
        cout<<"You don't have that much EXP!!";
        Sleep(3000);
        return;
    }
    int points=sacrificeExp/100;
    if(points<1) {
        heroEXP-=sacrificeExp;
        cout<<"Not enough sacrifices, come again when you're ready";
        Sleep(3000);
        return;
    }
    if(heroGold<points*100){
        cout<<"Not enough Gold! Go home";
        Sleep(3000);
        return;
    }
    showBar(200);
    heroDEF+=points*10*heroPath/heroLVL;
    heroEXP-=sacrificeExp;
    heroGold-=points*100;
}

void getSkill(string heroName,int menuGM3,int heroPath,int heroLVL,int &heroGold,int heroSkill[],int itemID[],string itemName[],string itemDesc[],int inventoryID[],int inventoryQTY[]){
    if(heroSkill[menuGM3]==1){
        cout<<"You already learn this skill!";
        Sleep(2000);
        return;
    }
    else{
        int itemSkill=menuGM3+31;
        bool reqItem=0,reqLvl=0,reqGold=0;
        if(heroLVL>=5*menuGM3) reqLvl=1;
        if(heroGold>(heroLVL*100*menuGM3)/heroPath) reqGold=1;
        for(int i=0;i<10;i++){
            if(inventoryID[i]==itemSkill) {reqItem=1;break;}
        }
        if(!reqLvl) {cout<<"Too young, come back later...";Sleep(2000);return;}
        else if(!reqItem) {cout<<"Bring me "<<itemDesc[cekID(itemSkill,itemID,20)];Sleep(2000);return;}
        else if(!reqGold) {cout<<"Give me Gold, lots of it...";Sleep(2000);return;}
        else{
            heroSkill[menuGM3]=1;
            showBar(300);
            cout<<"\nGo, Use this skill for good deeds...";
            Sleep(3000);
        }
    }
}
void restoration(string heroName,int heroPath,int heroMaxMP,int &heroMP,int heroMaxHP,int &heroHP,int heroLVL,int itemID[],string itemName[],int itemAttr[][3],int inventoryID[],int inventoryQTY[]){
    while(1){
        cls;
        showTitle();
        cout<<"R E S T O R A T I O N\n";
        heroStat1(heroName,heroMaxMP,heroMP,heroMaxHP,heroHP,0,0,0,heroLVL,0,0,heroPath,0);
        showBag(inventoryID,inventoryQTY,itemID,itemName,itemAttr);

        if(!yesno("Restore Health/Mana")) break;

        int foodID;
        cout<<"Item ID:";
        cin>>foodID;
        if(foodID/10!=9) {cout<<"Not a consumable item";Sleep(2000);continue;}
        if(cekID(foodID,inventoryID,10)==-1) {cout<<"You don't have that item";Sleep(2000);continue;}

        int addPoint;
        addPoint=id2Attr(foodID,itemID,itemAttr,0);
        if(addPoint!=0){
            heroHP+=addPoint;
            if(heroHP>heroMaxHP) heroHP=heroMaxHP;
        }

        addPoint=id2Attr(foodID,itemID,itemAttr,1);
        if(addPoint!=0){
            heroMP+=addPoint;
            if(heroMP>heroMaxMP) heroMP=heroMaxMP;
        }

        addPoint=id2Attr(foodID,itemID,itemAttr,2);
        if(addPoint!=0){
            heroHP=heroMaxHP;
            heroMP=heroMaxMP;
        }
        updBag(foodID,inventoryID,inventoryQTY,-1);
    }
}
void guildmasterNPC(string &heroName,int &heroMaxMP,int &heroMP,int &heroMaxHP,int &heroHP,int &heroDMG,int &heroDEF,int &heroLIFE,int heroLVL,int &heroEXP,int heroTNL,int heroPath,int &heroGold,int heroSkill[],string skillName[],string skillDesc[],int itemID[],string itemName[],int itemAttr[][3],string itemDesc[],int inventoryID[],int inventoryQTY[]){
    if(heroHP==0){
        if(heroGold<100) {
            cout<<"What an unfortunate warrior...I'm afraid I can not help you..."<<'\n';
            Sleep(3000);
            return;
        }
        cout<<"Great warrior, be careful next time ..."<<'\n';
        Sleep(1000);
        heroGold-=100;
        heroHP=100;
    }

    //tukar xp+gold->attr
    //get skill (heal own self, attack+stun(double turn), attack+counter(reflect monster damage),attack+crtdmg(critical),
    int menuGM;
    string guildMenu[5]={"Restoration","Build Charisma","Learn Skill","Change Name","EXIT"};
    while(1){
        cls;
        showTitle();

        heroStat1(heroName,heroMaxMP,heroMP,heroMaxHP,heroHP,heroDMG,heroDEF,heroLIFE,heroLVL,heroEXP,heroTNL,heroPath,heroGold);
        dispSkill(heroSkill,skillName,skillDesc);
        cout<<"\nGUILDMASTER"<<'\n';
        menuGM=menu(guildMenu,5);

        if(menuGM==1){
            restoration(heroName,heroPath,heroMaxMP,heroMP,heroMaxHP,heroHP,heroLVL,itemID,itemName,itemAttr,inventoryID,inventoryQTY);
        }
        else if(menuGM==2){
            if(heroLVL<10){
                cout<<"You are weak! Come again when you're stronger...";
                Sleep(3000);
                continue;
            }

            cout<<"\nCHARISMA BUILDER\n";
            string gm2Menu[5]={"HP","MP","Damage","Armor","EXIT"};
            int menuGM2=menu(gm2Menu,5);
            if(menuGM2==1) addHP(heroName,heroLVL,heroMaxHP,heroEXP,heroPath,heroGold);
            else if(menuGM2==2) addMP(heroName,heroLVL,heroMaxMP,heroEXP,heroPath,heroGold);
            else if(menuGM2==3) addDamage(heroName,heroLVL,heroDMG,heroEXP,heroPath,heroGold);
            else if(menuGM2==4) addArmor(heroName,heroLVL,heroDEF,heroEXP,heroPath,heroGold);
            else if(menuGM2==5) continue;
            else {cout<<"Select 1-5...";Sleep(2000);continue;}
        }
        else if(menuGM==3){
            if(heroLVL<5){
                cout<<"You're too young! Hunt some more...";
                Sleep(3000);
                continue;
            }

            cout<<"\nSKILL TRAINER\n";
            int menuGM3=menu2(skillName,5,skillDesc);

            if(menuGM3>=1 and menuGM3<=5) getSkill(heroName,menuGM3-1,heroPath,heroLVL,heroGold,heroSkill,itemID,itemName,itemDesc,inventoryID,inventoryQTY);
            else if(menuGM3==6) continue;
            else {cout<<"Chose 1-6...";Sleep(2000);continue;}

        }
        else if(menuGM==4){
            if(!yesno("The fee is 50000 gold coin, proceed")) continue;
            if(heroGold<50000){cout<<"You don't have enough gold";Sleep(3000);continue;}
            string newName;
            cout<<"\nEnter New Name (max 8 character) : ";
            cin.ignore(100,'\n');
            getline(cin,newName);
            heroName=newName.substr(0,8);
            heroGold-=50000;
        }
        else if(menuGM==5) break;
        else {cout<<"Choose 1-5...";Sleep(2000);continue;}
    }
}

void bankNPC(string heroName,int heroGold,bool &heroBank,int inventoryID[],int inventoryQTY[],int bankID[],int bankQTY[],int itemID[],string itemName[],int itemAttr[][3]){// inventory tambahan untuk menampung item karena bag penuh
    if(!heroBank) {
        warningMsg("You don't have Access to Bank",4);
        if(!yesno("\nDo you want to open an Account (cost:10000)")) return;
        if(heroGold<10000) warningMsg("You don't have enough Gold to open an account");
        heroBank=1;
    }
    cout<<"Welcome to UNIVERSAL BANK\n";
    cout<<"Safest place for your Artifact\n";
    Sleep(2000);
    while(1){
        cls;
        showTitle();
        cout<<"Hero : ";prncol(3);cout<<heroName;prncol(7);
        showBank(bankID,bankQTY,itemID,itemName,itemAttr);
        showBag(inventoryID,inventoryQTY,itemID,itemName,itemAttr);

        char ch;
        cout<<"(D)eposit (W)ithdraw or (C)ancel: "; cin>>ch; ch=toupper(ch);
        int id,qty;
        if(ch=='D' or ch=='W'){
            cout<<"Item ID#: ";cin>>id;
            cout<<"Quantity: ";cin>>qty;
        }
        else if(ch=='C') break;
        else {cout<<"Enter D,W,or C";Sleep(2000);continue;}

        int idxBag=cekID(id,inventoryID,10);
        int idxBank=cekID(id,bankID,10);

        if(ch=='D') {
            if(idxBag==-1) {warningMsg("No such item in BAG");continue;}
            else{
                if(inventoryQTY[idxBag]<qty){
                    warningMsg("You only have "+to_string(inventoryQTY[idxBag]));
                    continue;
                }
            }
            if(cekSlot(id,bankID,10)==-1) {warningMsg("Bank is Full");continue;}
            else{
                updBag(id,inventoryID,inventoryQTY,-qty);
                updBag(id,bankID,bankQTY,qty);
            }
        }
        else if(ch=='W'){
            if(idxBank==-1) {warningMsg("No such item in BANK");continue;}
            else{
                if(bankQTY[idxBank]<qty){
                    warningMsg("BANK only have "+to_string(bankQTY[idxBank]));
                    continue;
                }
            }
            if(cekSlot(id,inventoryID,10)==-1) {warningMsg("Bag is Full");continue;}
            else{
                updBag(id,inventoryID,inventoryQTY,qty);
                updBag(id,bankID,bankQTY,-qty);
            }
        }
    }
}
